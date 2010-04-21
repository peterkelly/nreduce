import java.util.PriorityQueue;
import java.util.List;
import java.util.LinkedList;
import java.util.Random;

public class Simulator
{
  public PriorityQueue<Event> queue = new PriorityQueue<Event>();
  public int time = 0;
  public Node[] nodes;
  public Random rand = new Random();
  public int framesRemaining = 0;
  public int totalFish = 0;
  public int totalSchedule = 0;
  public int totalScheduledFrames = 0;

  public int lastFish = 0;
  public int lastSchedule = 0;
  public int lastScheduledFrames = 0;

  public int TRANSMIT_TIME = 100;
  public int MAX_RUNNABLE = 2;
  public int LOG_FREQUENCY = 1000;
  public int HUNGRY_INTERVAL = 250;
  public int FRAME_DURATION = 1000;
  public int TOTAL_FRAMES = 4096;

  public boolean showLog;
  public boolean finished = false;

  class Event implements Comparable<Event>
  {
    public int timestamp;

    public Event(int timestamp)
    {
      this.timestamp = timestamp;
    }

    public int compareTo(Event other)
    {
      return timestamp - other.timestamp;
    }
  }

  class LogEvent extends Event
  {
    public LogEvent(int timestamp)
    {
      super(timestamp);
    }
  }

  class NodeEvent extends Event
  {
    public Node node;

    public NodeEvent(int timestamp, Node node)
    {
      super(timestamp);
      assert(node != null);
      this.node = node;
    }
  }

  class MessageEvent extends NodeEvent
  {
    public MessageEvent(Node dest)
    {
      super(time+TRANSMIT_TIME/2+rand.nextInt(1000)*TRANSMIT_TIME/1000,dest);
    }
  }

  class FishMessage extends MessageEvent
  {
    public Node requester;
    public int ttl;

    public FishMessage(Node node, Node requester, int ttl)
    {
      super(node);
      assert(requester != null);
      this.requester = requester;
      this.ttl = ttl;
      totalFish++;
    }
  }

  class ScheduleMessage extends MessageEvent
  {
    public List<Frame> frames;

    public ScheduleMessage(Node node, List<Frame> frames)
    {
      super(node);
      this.frames = frames;
      totalSchedule++;
      totalScheduledFrames += frames.size();
    }
  }

  class FrameCompletionEvent extends NodeEvent
  {
    public Frame frame;

    public FrameCompletionEvent(int timestamp, Node node, Frame frame)
    {
      super(timestamp,node);
      this.frame = frame;
    }
  }

  class HungryEvent extends NodeEvent
  {
    public HungryEvent(int timestamp, Node node)
    {
      super(timestamp,node);
    }
  }

  class Frame
  {
    public int duration;

    public Frame(int duration)
    {
      this.duration = duration;
    }
  }

  class Node
  {
    public List<Frame> sparks = new LinkedList<Frame>();
    public List<Frame> runnable = new LinkedList<Frame>();
    public int index;

    public Node(int index)
    {
      this.index = index;
    }

    public void handle(NodeEvent event)
    {
      if (event instanceof FrameCompletionEvent)
        handleFrameCompletion((FrameCompletionEvent)event);
      else if (event instanceof FishMessage)
        handleFish((FishMessage)event);
      else if (event instanceof ScheduleMessage)
        handleSchedule((ScheduleMessage)event);
      else if (event instanceof HungryEvent)
        handleHungry((HungryEvent)event);
      else
        throw new RuntimeException("Unknown event type: "+event.getClass().getName());
    }

    public void handleFrameCompletion(FrameCompletionEvent event)
    {
      assert(runnable.contains(event.frame));
      runnable.remove(event.frame);
      framesRemaining--;
      checkRunnable();
    }

    public void handleFish(FishMessage message)
    {
      if (!sparks.isEmpty()) {
//         int toSchedule = Math.min(128,sparks.size());
        int toSchedule = sparks.size()/2;
        if (toSchedule > 0) {
          List<Frame> frames = new LinkedList<Frame>();
          for (int i = 0; i < toSchedule; i++)
            frames.add(sparks.remove(0));
          postEvent(new ScheduleMessage(message.requester,frames));
        }
      }
      else if (message.ttl > 1) {
        int dest = rand.nextInt(nodes.length);
        postEvent(new FishMessage(nodes[dest],message.requester,message.ttl-1));
      }
    }

    public void handleSchedule(ScheduleMessage message)
    {
      sparks.addAll(message.frames);
      checkRunnable();
    }

    public void handleHungry(HungryEvent event)
    {
      if (sparks.isEmpty() && runnable.isEmpty()) {
//       if (runnable.size() < MAX_RUNNABLE) {
        int dest = rand.nextInt(nodes.length);
//         System.out.println(index+": sending fish");
        postEvent(new FishMessage(nodes[dest],this,nodes.length));
      }
      postEvent(new HungryEvent(time+HUNGRY_INTERVAL+rand.nextInt(1000)*HUNGRY_INTERVAL/1000,this));
      checkRunnable();
    }

    public void checkRunnable()
    {
      while (!sparks.isEmpty() && (runnable.size() < MAX_RUNNABLE)) {
        Frame frame = sparks.remove(0);
        runnable.add(frame);
        postEvent(new FrameCompletionEvent(time+frame.duration,this,frame));
      }
    }
  }

  public Simulator(boolean showLog)
  {
    this.showLog = showLog;
  }

  public void postEvent(Event event)
  {
    queue.add(event);
  }

  public void done()
  {
    if (showLog) {
      System.out.println("Completion time = "+time);
      System.out.println("Total FISH messages = "+totalFish);
      System.out.println("Total SCHEDULE messages = "+totalSchedule);
      System.out.println("Total scheduled frames = "+totalScheduledFrames);
      System.out.println("Average migrations per frame = "+totalScheduledFrames/TOTAL_FRAMES);
    }
    finished = true;
  }

  public int[] getResults()
  {
    return new int[]{time,
                     totalFish,
                     totalSchedule,
                     totalScheduledFrames,
                     totalScheduledFrames/TOTAL_FRAMES};
  }

  public void log()
  {
    int activeNodes = 0;
    int nodesWithSparks = 0;
    for (int i = 0; i < 32; i++) {
      if (!nodes[i].runnable.isEmpty())
        activeNodes++;
      if (!nodes[i].sparks.isEmpty())
        nodesWithSparks++;
    }

//     int utilisation = 100*activeNodes/nodes.length;

    int fish = totalFish - lastFish;
    int schedule = totalSchedule - lastSchedule;
    int scheduledFrames = totalScheduledFrames - lastScheduledFrames;

    lastFish = totalFish;
    lastSchedule = totalSchedule;
    lastScheduledFrames = totalScheduledFrames;

    if (showLog) {
      System.out.format("%-8d ",time);
      System.out.format("%-4d ",framesRemaining);
      System.out.format("%-3d ",activeNodes);
      System.out.format("%-3d ",nodesWithSparks);
      System.out.format("%-4d ",fish);
      System.out.format("%-4d ",schedule);
      System.out.format("%-4d ",scheduledFrames);

      System.out.format("| ");

      for (int i = 0; i < 32; i++)
        System.out.format("%-4d ",nodes[i].sparks.size());
      System.out.format("\n");
    }

    if (framesRemaining == 0)
      done();

    postEvent(new LogEvent(time+LOG_FREQUENCY));
  }

  public void run()
  {
    nodes = new Node[32];
    for (int i = 0; i < nodes.length; i++) {
      nodes[i] = new Node(i);
      postEvent(new HungryEvent(time+HUNGRY_INTERVAL+rand.nextInt(1000)*HUNGRY_INTERVAL/1000,
                                nodes[i]));
    }
    for (int i = 0; i < TOTAL_FRAMES; i++) {
      nodes[0].sparks.add(new Frame(FRAME_DURATION));
      framesRemaining++;
    }

    log();

    while (!queue.isEmpty() && !finished) {
      Event event = queue.poll();
      time = event.timestamp;
      if (event instanceof NodeEvent) {
        NodeEvent nodeEvent = (NodeEvent)event;
        nodeEvent.node.handle(nodeEvent);
      }
      else if (event instanceof LogEvent) {
        log();
      }
    }
  }

  public void setParameter(String name, int value)
  {
    if (name.equalsIgnoreCase("TRANSMIT_TIME"))
      TRANSMIT_TIME = value;
    else if (name.equalsIgnoreCase("MAX_RUNNABLE"))
      MAX_RUNNABLE = value;
    else if (name.equalsIgnoreCase("LOG_FREQUENCY"))
      LOG_FREQUENCY = value;
    else if (name.equalsIgnoreCase("HUNGRY_INTERVAL"))
      HUNGRY_INTERVAL = value;
    else if (name.equalsIgnoreCase("FRAME_DURATION"))
      FRAME_DURATION = value;
    else if (name.equalsIgnoreCase("TOTAL_FRAMES"))
      TOTAL_FRAMES = value;
    else
      throw new RuntimeException("Unknown parameter: "+name);
  }

/*

Single-run:
X-axis: time
Y-axis: utilisation, nodesWithSparks

Multiple runs:
X-axis: transmit time, hungry interval, frame distribution count, ttl, frame duration, #nodes
Y-axis: completion time, #fish, #schedule #schedframes, avg migrations

Algorithm variations:
Schedule N sparks
Schedule half sparks
Low/high watermark?

 */

  public static void runMultiple(String pname, int xmin, int xmax, int xincr)
  {
    int runs = 10;
    int NUM_RESULTS = 5;


    System.out.format("# "+pname+" time fish schedule sframes avgmig\n");

    for (int x = xmin; x <= xmax; x += xincr) {
      int[] ytotals = new int[NUM_RESULTS];
      for (int r = 0; r < runs; r++) {
        Simulator simulator = new Simulator(false);
        simulator.setParameter(pname,x);
        simulator.run();
        int[] results = simulator.getResults();
        for (int v = 0; v < results.length; v++)
          ytotals[v] += results[v];
      }
      int[] yaverages = new int[NUM_RESULTS];
      for (int v = 0; v < ytotals.length; v++)
        yaverages[v] = ytotals[v]/runs;

      System.out.format("%-6d ",x);
      for (int v = 0; v < yaverages.length; v++)
        System.out.format("%-6d ",yaverages[v]);
      System.out.format("\n");
    }
  }

  public static void main(String[] args)
  {
    if ((args.length == 5) && args[0].equals("-multiple")) {
      String pname = args[1];
      int xmin = Integer.parseInt(args[2]);
      int xmax = Integer.parseInt(args[3]);
      int xincr = Integer.parseInt(args[4]);
      runMultiple(pname,xmin,xmax,xincr);
    }
    else if (args.length == 0) {
      Simulator simulator = new Simulator(true);
      simulator.run();
    }
    else {
      System.err.println("Invalid arguments");
      System.exit(1);
    }
  }
}
