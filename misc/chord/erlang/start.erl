-module(start).
-export([domain/0,dotest/0]).

domain() ->
  compile:file(chord),
  chord:main().

dotest() ->
  compile:file(chord),
  chord:test().
