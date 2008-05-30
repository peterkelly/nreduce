[
   Sym                 -- _1,
   Nl                  -- KW["nil"],
   Number              -- _1,
   Str                 -- H hs=0 [KW["\""] _1 KW["\""]],
   Binding             -- H[_1 KW["="] _2],
   Letrec              -- H hs=0 [KW["("] H hs=1 [
V vs=0 [V is=2 [KW["letrec"] _1] V is=2 [KW["in"] H hs=0 [KW["("] _2 KW[")"]]]]
] KW[")"]],
   Letrec.1:iter       -- V[_1],
   App                 -- H hs=1 [_1 _2],
   Lambda              -- H hs=0 [KW["!"] _1 KW["."] H hs=1 [_2]],

   ListApp             -- H hs=0 [KW[""] V is=2 [_1] KW[""]],
   ListApp.1:iter      -- _1,
   SmallApp            -- H hs=0 [KW["("] H hs=1 [_1] KW[")"]],
   SmallApp.1:iter     -- _1,

   ListLambda          -- H hs=0 [KW["("] V is=1 [H hs=0 [_1] _2] KW[")"]],
   ListLambda.1:iter   -- _1,
   Entry               -- H hs=0 [KW["!"] _1 KW["."]],

   SCombNB             -- V[H hs=1 [_1 _2 KW["="]] _3],
   SCombNB.2:iter-star -- _1,
   SComb               -- V[H hs=1 [_1 _2 KW["="]] H hs=0 [KW["("] _3 KW[")"]]],
   SComb.2:iter-star   -- _1,
   SCombConst               -- H hs=1 [_1 _2 KW["="] _3],
   SCombConst.2:iter-star   -- _1,
   Parenthetical       -- H hs=0 ["(" _1 ")"],
   Import              -- H hs=1 [KW["import"] _1],
   Program             -- V vs=1 [V vs=0 [_1] V vs=1 [_2]],
   Program.1:iter-star -- _1,
   Program.2:iter-star -- _1
]
