-module(chord).
-export([main/0,
         join/2,
         stabilizer/1,
         debug_loop/1,
         manager/0,
         find_all/1]).

-define(STABILIZER_DELAY,1000).
-define(DEBUG_DELAY,250).
-define(NODE_COUNT,32).

-define(MBITS,8).
-define(KEYSPACE,256).

%-define(MBITS,12).
%-define(KEYSPACE,4096).

%-define(MBITS,16).
%-define(KEYSPACE,65536).

-record(node,{id,pid}).

%%==============================================================================
% Debug routines

print_finger(H) ->
    if H == none ->
            io:format("      -");
       true ->
            io:format(" ~.6w",[H#node.id])
    end.


print_node_fingers(Node) ->
    Node#node.pid ! {get_fingers,self()},
    {reply_fingers,Predecessor,Fingers} = receive X -> X end,
    NodeId = Node#node.id,
    io:format("~.6w| ",[NodeId]),
    lists:foreach(fun print_finger/1,Fingers),
    if Predecessor == none ->
            io:format("    (no pred)");
       true ->
            io:format("    (pred ~w)",[Predecessor#node.id])
    end,
    io:format("~n").


check_keyspace(Hook,Id,Nodes,BadMatches,TotalHops) ->
    if Id >= ?KEYSPACE ->
            {BadMatches,TotalHops};
       true ->
            Expected = manual_hash(Id,Nodes,hd(Nodes)),
            Hook ! {find_successor,Id,self(),0,none},
            {Actual,Hops} =
                receive
                    {got_successor,Successor,Hops1,_} ->
                        {Successor,Hops1}
                end,
            if Expected == Actual#node.id ->
                    check_keyspace(Hook,Id+1,Nodes,BadMatches,TotalHops+Hops);
               true ->
                    check_keyspace(Hook,Id+1,Nodes,BadMatches+1,TotalHops+Hops)
            end
    end.


manual_hash(Id,[Node|Rest],FirstNode) ->
    if Id =< Node#node.id ->
            Node#node.id;
       true ->
            manual_hash(Id,Rest,FirstNode)
    end;
manual_hash(_,[],FirstNode) ->
    FirstNode#node.id.


check_successor(Index,Nodes,Fingers) ->
    Expected =
        if Index < length(Nodes) ->
                lists:nth(Index+1,Nodes);
           true ->
                lists:nth(1,Nodes)
        end,
    Actual = lists:nth(1,Fingers),
    if Expected == Actual ->
            0;
       true ->
            1
    end.

check_predecessor(Index,Nodes,Predecessor) ->
    Expected =
        if Index > 1 ->
                lists:nth(Index-1,Nodes);
           true ->
                lists:nth(length(Nodes),Nodes)
        end,
    if Expected == Predecessor ->
            0;
       true ->
            1
    end.

check_fingers(N,K,Nodes,Fingers,BadFingers) ->
    if K > ?MBITS ->
            BadFingers;
       true ->
            FingerKStart = N+math:pow(2,K-1),
            Expected = manual_hash(FingerKStart,Nodes,hd(Nodes)),
            Actual = lists:nth(K,Fingers),
            if Actual == none ->
                    check_fingers(N,K+1,Nodes,Fingers,BadFingers+1);
               Expected == Actual#node.id ->
                    check_fingers(N,K+1,Nodes,Fingers,BadFingers);
               true ->
                    check_fingers(N,K+1,Nodes,Fingers,BadFingers+1)
            end
    end.

check_loop(Index,Nodes,Incorrect,BadSuc,BadPred,BadFingers,BadLookup,TotalHops,StartTime) ->
    if Index > length(Nodes) ->
            {_, Secs, MicroSecs} = now(),
            Time = Secs + MicroSecs/1000000,
            RelativeTime = Time - StartTime,
            io:format("~.3f ~.6f ~.6f ~.6f ~.6f ~.6f ~6w check~n",
                      [RelativeTime,
                       100.0*Incorrect/?NODE_COUNT,
                       100.0*BadSuc/?NODE_COUNT,
                       100.0*BadPred/?NODE_COUNT,
                       100.0*BadFingers/(?NODE_COUNT*?MBITS),
                       100.0*BadLookup/(?NODE_COUNT*?KEYSPACE),
                       TotalHops]);
       true ->
            Node = lists:nth(Index,Nodes),
            {Bad,Hops} = check_keyspace(Node#node.pid,0,Nodes,0,0),

            Node#node.pid ! {get_fingers,self()},
            {Predecessor,Fingers} =
                receive
                    {reply_fingers,Predecessor1,Fingers1} ->
                        {Predecessor1,Fingers1}
                end,
            MatchesSuc = check_successor(Index,Nodes,Fingers),
            MatchesPred = check_predecessor(Index,Nodes,Predecessor),
            MatchesFingers = check_fingers(Node#node.id,1,Nodes,Fingers,0),
            NewIncorrect =
                if Bad == 0 ->
                        Incorrect;
                   true ->
                        Incorrect+1
                end,
            check_loop(Index+1,
                       Nodes,
                       NewIncorrect,
                       BadSuc+MatchesSuc,
                       BadPred+MatchesPred,
                       BadFingers+MatchesFingers,
                       BadLookup+Bad,
                       TotalHops+Hops,
                       StartTime)
    end.

debug_loop1(Nodes,StartTime,Iterations) ->
    receive
        pause ->
            receive
                resume ->
                    ok
            end
    after ?DEBUG_DELAY ->
            ok
    end,
%    lists:foreach(fun print_node_fingers/1,Nodes),
    check_loop(1,Nodes,0,0,0,0,0,0,StartTime),

    if Iterations == 6 ->
            io:format("Killing node ~w~n",[lists:nth(1,Nodes)]),
            (lists:nth(1,Nodes))#node.pid ! {kill};
       true ->
            ok
    end,

    debug_loop1(Nodes,StartTime,Iterations+1).

debug_loop(Nodes) ->
    {_, Secs, MicroSecs} = now(),
    StartTime = Secs + MicroSecs/1000000,
    debug_loop1(lists:sort(Nodes),StartTime,0).

%%==============================================================================
% Util

replace([_|T],0,Replacement) ->
    [Replacement|T];
replace([H|T],Index,Replacement) ->
    [H|replace(T,Index-1,Replacement)].

inrange_open(Val,Min,Max) ->
    if (Min < Max) ->
            ((Val > Min) and (Val < Max));
       true ->
            ((Val > Min) or (Val < Max))
    end.

inrange_open_closed(Val,Min,Max) ->
    if (Min < Max) ->
            ((Val > Min) and (Val =< Max));
       true ->
            ((Val > Min) or (Val =< Max))
    end.

%%==============================================================================
% Core Chord functionality

set_finger(Fingers,Index,Replacement) ->
    replace(Fingers,Index-1,Replacement).

closest_preceding_finger(NodeId,_,0,_) ->
    #node{id=NodeId,pid=self()};
closest_preceding_finger(NodeId,Fingers,FIndex,Id) ->
    CurFinger = lists:nth(FIndex,Fingers),
    if CurFinger == none ->
            closest_preceding_finger(NodeId,Fingers,FIndex-1,Id);
       true ->
            #node{id=FingerId,pid=_} = CurFinger,
            Res = inrange_open(FingerId,NodeId,Id),
            if Res ->
                    CurFinger;
               true ->
                    closest_preceding_finger(NodeId,Fingers,FIndex-1,Id)
            end
    end.

cnode(NodeId,Predecessor,Fingers) ->
    receive
        % lookup
        {find_successor,Id,Sender,Hops,Payload} ->
            Successor = lists:nth(1,Fingers),

            if Successor /= none ->
                    InRange = inrange_open_closed(Id,NodeId,Successor#node.id),
                    if InRange ->
                            Sender ! {got_successor,Successor,Hops,Payload};
                       true ->
                            Closest = closest_preceding_finger(NodeId,Fingers,?MBITS,Id),
                            Closest#node.pid ! {find_successor,Id,Sender,Hops+1,Payload}
                    end,
                    cnode(NodeId,Predecessor,Fingers)
            end;

        % stabilization

        {notify,Ndash} ->
%            Ndash#node.pid ! {notify_reply,Predecessor},
            Successor = lists:nth(1,Fingers),
            NewFingers =
                if Successor#node.id == NodeId ->
                        set_finger(Fingers,1,Ndash);
                   true ->
                        Fingers
                end,

            if Predecessor == none ->
                    io:format("~.6w:    notify: predecessor -> ~w~n",[NodeId,Ndash#node.id]),
                    Ndash#node.pid ! {notify_reply,Ndash},
                    cnode(NodeId,Ndash,NewFingers);
               true ->
                    InRange = inrange_open(Ndash#node.id,Predecessor#node.id,NodeId),
                    if InRange ->
                            io:format("~.6w:    notify: predecessor ~w -> ~w~n",
                                      [NodeId,Predecessor#node.id,Ndash#node.id]),
                            Predecessor#node.pid ! {stabilize1},
                            Ndash#node.pid ! {notify_reply,Ndash},
                            cnode(NodeId,Ndash,NewFingers);
                       true ->
                            Ndash#node.pid ! {notify_reply,Predecessor},
                            cnode(NodeId,Predecessor,NewFingers)
                    end
            end;

        {notify_reply,SucPrec} ->

            if SucPrec /= none ->
                    Successor = lists:nth(1,Fingers),
                    InRange = inrange_open(SucPrec#node.id,NodeId,Successor#node.id),
                    if InRange ->
                            io:format("~.6w: stabilize: successor ~w -> ~w E (~w,~w)~n",
                                      [NodeId,Successor#node.id,SucPrec#node.id,NodeId,
                                       Successor#node.id]),
                            NewFingers = set_finger(Fingers,1,SucPrec),
                            SucPrec#node.pid ! {notify,#node{id=NodeId,pid=self()}},
                            cnode(NodeId,Predecessor,NewFingers);
                       true ->
                            cnode(NodeId,Predecessor,Fingers)
                    end;
               true ->
                    cnode(NodeId,Predecessor,Fingers)
            end;

        {got_successor,F,_,{fupdate,K}} ->
            NewFingers = set_finger(Fingers,K,F),
            cnode(NodeId,Predecessor,NewFingers);

        {update_fingers} ->
%            finger_update(NodeId,2),
            K = 1+random:uniform(?MBITS-1),
            FingerKStart = NodeId+math:pow(2,K-1),
            self() ! {find_successor,FingerKStart,self(),0,{fupdate,K}},
            cnode(NodeId,Predecessor,Fingers);

        {stabilize1} ->
            Successor = lists:nth(1,Fingers),
            Successor#node.pid ! {notify,#node{id=NodeId,pid=self()}},
            cnode(NodeId,Predecessor,Fingers);

        % debugging
        {get_fingers,Sender} ->
            Sender ! {reply_fingers,Predecessor,Fingers},
            cnode(NodeId,Predecessor,Fingers);

        {find_all,Sender,Ids,FirstId} ->
            io:format("~w: find_all: have ~w ids so far~n",[NodeId,length(Ids)]),
            Successor = lists:nth(1,Fingers),
            MyId = #node{id=NodeId,pid=self()},
            if FirstId == NodeId ->
                    Sender ! {got_all,Ids};
               FirstId == none ->
                    Successor#node.pid ! {find_all,Sender,[MyId|Ids],NodeId};
               true ->
                    Successor#node.pid ! {find_all,Sender,[MyId|Ids],FirstId}
            end,
            cnode(NodeId,Predecessor,Fingers);

        {kill} ->
            io:format("~w: node failure~n",[NodeId]),
            exit(node_failure);

        {'EXIT',FromPID,_} ->
            io:format("~w: got EXIT for pid ~w~n",[NodeId,FromPID]),
            cnode(NodeId,Predecessor,Fingers)

    end.

stabilizer1(Node) ->
    Node#node.pid ! {stabilize1},
    Node#node.pid ! {update_fingers},
%    receive after ?STABILIZER_DELAY -> ok end,
    Delay = trunc(?STABILIZER_DELAY/2 + random:uniform(?STABILIZER_DELAY)),
%    Delay = ?STABILIZER_DELAY,
%    io:format("stabilizer ~w: delay ~w~n",[Node#node.id,Delay]),
    receive after Delay -> ok end,
    stabilizer1(Node).

stabilizer(Node) ->
    random:seed(0,0,Node#node.id),
    random:uniform(),
    stabilizer1(Node).

join(NodeId,Ndash) ->
    process_flag(trap_exit,true),
    {MegaSecs,Secs,MicroSecs} = now(),
    random:seed(MegaSecs,Secs,MicroSecs),
    random:uniform(),
    Fingers = init_finger_table(),
    Node = #node{id=NodeId,pid=self()},
    Successor =
        if Ndash == none ->
                io:format("~w: join; i am ~w; no exsting node~n",[NodeId,self()]),
                Node;
           true ->
                io:format("~w: join; i am ~w; using Ndash ~w~n",[NodeId,self(),Ndash]),
                Ndash#node.pid ! {find_successor,NodeId,self(),0,none},
                receive {got_successor,ReceivedSuccessor,_,_} ->
                        io:format("~w: initial successor = ~w~n",[NodeId,Ndash]),
                        ReceivedSuccessor
                end
        end,
    NewFingers = set_finger(Fingers,1,Successor),
    spawn(chord,stabilizer,[Node]),
    cnode(NodeId,none,NewFingers).

%%==============================================================================
% Setup

invent_node_ids(Count,Ids) ->
    if Count == 0 ->
            Ids;
       true ->
            NewId = random:uniform(?KEYSPACE),
            Have = lists:member(NewId,Ids),
            if Have ->
                    io:format("already have ~w~n",[NewId]),
                    invent_node_ids(Count,Ids);
               true ->
                    invent_node_ids(Count-1,[NewId|Ids])
            end
    end.

start_node(Id,Hook) ->
    Pid = spawn(chord,join,[Id,Hook]),
    #node{id=Id,pid=Pid}.

start_remaining([Id|Rest],Initial) ->
    [start_node(Id,Initial)|start_remaining(Rest,Initial)];
start_remaining([],_) ->
    [].

init_nodes() ->
    Ids = invent_node_ids(?NODE_COUNT,[]),
    Initial = start_node(hd(Ids),none),
    {Ids,[Initial|start_remaining(tl(Ids),Initial)]}.

%%==============================================================================
% Program control

init_finger_table1(Remaining) ->
    if Remaining == 0 ->
            [];
       true ->
            [none|init_finger_table1(Remaining-1)]
    end.

init_finger_table() ->
    init_finger_table1(?MBITS).

manager_loop(Nodes,LookupPid) ->
    receive
        pause_info ->
            LookupPid ! pause;
        resume_info ->
            LookupPid ! resume;
        {find_all,Sender} ->
            (hd(Nodes))#node.pid ! {find_all,Sender,[],none}
    end,
    manager_loop(Nodes,LookupPid).

find_all(ManagerPid) ->
    ManagerPid ! {find_all,self()},
    receive
        {got_all,Ids} ->
            Ids
    end.

manager() ->
    io:format("Starting~n"),

    {_,Nodes} = init_nodes(),

    LookupPid = spawn(chord,debug_loop,[Nodes]),

    manager_loop(Nodes,LookupPid).

main() ->
    spawn(chord,manager,[]).
