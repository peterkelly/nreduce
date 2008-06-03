[
   LibraryModule                              -- _1 _2 _3,
   LibraryModule.1:opt                        -- _1,
   MainModule                                 -- V vs=1 [_1 _2 _3],
   MainModule.1:opt                           -- _1,
   VersionDecl                                -- H hs=0 [H[KW["xquery"] KW["version"] _1 _2] KW[";"]],
   VersionDecl.2:opt                          -- _1,
   VersionDecl.2:opt.1:seq                    -- H hs=1 [KW["encoding"] _1],
   ModuleDecl                                 -- H hs=1 [KW["module"] KW["namespace"] _1 KW["="] _2 KW[";"]],
   Prolog                                     -- _1 _2,
   Prolog.1:iter-star                         -- _1,
   Prolog.2:iter-star                         -- _1,
   NamespaceDecl                              -- H hs=1 [KW["declare"] KW["namespace"] _1 KW["="] H hs=0 [_2 KW[";"]]],
   BoundarySpaceDecl                          -- H hs=1 [KW["declare"] KW["boundary-space"] H hs=0 [_1 KW[";"]]],
   BoundarySpaceDecl.1:alt                    -- KW["preserve"] KW["strip"],
   DefaultElemNS                              -- H hs=1 [KW["declare"] KW["default"] KW["element"] KW["namespace"] H hs=0 [_1 KW[";"]]],
   DefaultFunNS                               -- H hs=1 [KW["declare"] KW["default"] KW["function"] KW["namespace"] H hs=0 [_1 KW[";"]]],
   OptionDecl                                 -- H hs=1 [KW["declare"] KW["option"] _1 _2 KW[";"]],
   OrderingModeDecl                           -- H hs=1 [KW["declare"] KW["ordering"] H hs=0 [_1 KW[";"]]],
   OrderingModeDecl.1:alt                     -- KW["ordered"] KW["unordered"],
   EmptyOrderDecl                             -- H hs=1 [KW["declare"] KW["default"] KW["order"] KW["empty"] H hs=0 [_1 KW[";"]]],
   EmptyOrderDecl.1:alt                       -- KW["greatest"] KW["least"],
   CopyNamespacesDecl                         -- H hs=1 [KW["declare"] KW["copy-namespaces"] H hs=0 [_1 KW[","]] H hs=0 [ _2 KW[";"]]],
   PreserveMode                               -- _1,
   PreserveMode.1:alt                         -- KW["preserve"] KW["no-preserve"],
   InheritMode                                -- _1,
   InheritMode.1:alt                          -- KW["inherit"] KW["no-inherit"],
   DefaultCollationDecl                       -- H hs=1 [KW["declare"] KW["default"] KW["collation"] H hs=0 [_1 KW[";"]]],
   BaseURIDecl                                -- H hs=1 [KW["declare"] KW["base-uri"] H hs=0 [_1 KW[";"]]],
   SchemaImport                               -- H hs=1 [KW["import"] KW["schema"] H hs=0 [H hs=1 [_1 _2 _3] KW[";"]]],
   SchemaImport.1:opt                         -- _1,
   SchemaImport.3:opt                         -- _1,
   URIs                                       -- _1,
   URIs.1:seq                                 -- H hs=1 [KW["at"] _1],
   URIs.1:seq.1:iter-sep                      -- H hs=0 [_1 KW[","]],
   SchemaPrefix                               -- KW["namespace"] _1 KW["="],
   DefaultSchemaPrefix                        -- KW["default"] KW["element"] KW["namespace"],
   ModuleImport                               -- H hs=1 [KW["import"] KW["module"] H hs=0 [H hs=1 [_1 _2 _3] KW[";"]]],
   ModuleImport.1:opt                         -- _1,
   ModuleImport.1:opt.1:seq                   -- KW["namespace"] _1 KW["="],
   ModuleImport.3:opt                         -- _1,
   ServiceImport                              -- H hs=1 [KW["import"] KW["service"] KW["namespace"] _1 KW["="] H hs=0 [_2 KW[";"]]],
   VarDecl                                    -- H hs=1 [KW["declare"] KW["variable"] H hs=0 [H hs=1 [H hs=0 [KW["$"] _1] _2 _3] KW[";"]]],
   VarDecl.2:opt                              -- _1,
   VarDecl.3:seq                              -- KW[":="] _1,
   ExternalVarDecl                            -- H hs=1 [KW["declare"] KW["variable"] H hs=0 [H hs=1 [H hs=0 [KW["$"] _1] _2 KW["external"]] KW[";"]]],
   ExternalVarDecl.2:opt                      -- _1,
   Construction                               -- H hs=1 [KW["declare"] KW["construction"] H hs=0 [_1 KW[";"]]],
   Construction.1:alt                         -- KW["strip"] KW["preserve"],
   Function                                   -- V[H[KW["declare"] KW["function"] H hs=0 [_1 KW["("] H hs=1 [_2] KW[")"]] _3] H hs=0 [_4 KW[";"]]],
   Function.2:iter-star-sep                   -- H hs=0 [_1 KW[","]],
   Function.3:opt                             -- _1,
   ExternalFunction                           -- H hs=1 [KW["declare"] KW["function"] _1 KW["("] _2 KW[")"] _3 KW["external"] KW[";"]],
   ExternalFunction.2:iter-star-sep           -- _1 KW[","],
   ExternalFunction.3:opt                     -- _1,
   Param                                      -- H hs=1 [H hs=0 [KW["$"] _1] _2],
   Param.2:opt                                -- _1,
   ValidationMode                             -- _1,
   ValidationMode.1:alt                       -- KW["lax"] KW["strict"],
   Pragma                                     -- KW["(#"] _1 _2 _3 KW["#)"],
   Pragma.1:opt                               -- _1,
   Pragma.3:opt                               -- _1,
   Pragma.3:opt.1:seq                         -- _1 _2,
   Enclosed                                   -- V[V is=2 [KW["{"] H[_1]] KW["}"]],
   For                                        -- V  [H  [KW["for"] V[_1]]],
   For.1:iter-sep                             -- H hs=0 [_1 KW[","]],
   ForPart                                    -- H[H hs=0 [KW["$"] _1] _2 _3 KW["in"] _4],
   ForPart.2:opt                              -- _1,
   ForPart.3:opt                              -- _1,
   Let                                        -- V  [H  [KW["let"] V[_1]]],
   Let.1:iter-sep                             -- H hs=0 [_1 KW[","]],
   LetPart                                    -- H[H hs=0 [KW["$"] _1] _2 KW[":="] _3],
   LetPart.2:opt                              -- _1,
   PositionalVar                              -- KW["at"] KW["$"] _1,
   Where                                      -- H[KW["where"] _1],
   OrderBy                                    -- H[KW["order"] KW["by"] _1],
   OrderBy.1:iter-sep                         -- _1 KW[","],
   StableOrderBy                              -- V  [H  [KW["stable"] KW["order"] KW["by"]] _1],
   StableOrderBy.1:iter-sep                   -- _1 KW[","],
   OrderSpec                                  -- _1 _2,
   OrderModifier                              -- _1 _2 _3,
   OrderModifier.1:opt                        -- _1,
   OrderModifier.1:opt.1:alt                  -- KW["ascending"] KW["descending"],
   OrderModifier.2:opt                        -- _1,
   OrderModifier.2:opt.1:seq                  -- KW["empty"] _1,
   OrderModifier.2:opt.1:seq.1:alt            -- KW["greatest"] KW["least"],
   OrderModifier.3:opt                        -- _1,
   OrderModifier.3:opt.1:seq                  -- KW["collation"] _1,
   Case1                                      -- KW["case"] _1 KW["return"] _2,
   Case2                                      -- KW["case"] KW["$"] _1 KW["as"] _2 KW["return"] _3,
   ForwardStep                                -- _1 _2,
   ForwardStepChild                           -- _1,
   ForwardStepAttr                            -- KW["@"] _1,
   ReverseStep                                -- _1 _2,
   ReverseStepParent                          -- KW[".."],
   String                                     -- _1,
   Integer                                    -- _1,
   Decimal                                    -- _1,
   Double                                     -- _1,
   VarRef                                     -- H hs=0 [KW["$"] _1],
   EmptySeq                                   -- H hs=0 [KW["("] KW[")"]],
   ContextItem                                -- KW["."],
   Call                                       -- H hs=0 [_1 KW["("] _2 KW[")"]],
   Call.2:iter-star-sep                       -- _1 KW[","],
   FunctionName                               -- _1,
   Ordered                                    -- H hs=1 [KW["ordered"] KW["{"] _1 KW["}"]],
   Unordered                                  -- H hs=1 [KW["unordered"] KW["{"] _1 KW["}"]],
   Predicate                                  -- H hs=0 [KW["["] _1 KW["]"]],
   Filter                                     -- H hs=0 [_1 _2],
   ForwardAxisStep                            -- _1,
   ReverseAxisStep                            -- _1,
   Step                                       -- H hs=0 [_1 KW["/"] _2],
   Step2                                      -- H hs=0 [_1 KW["/"] KW["/"] _2],
   Validate                                   -- KW["validate"] _1 KW["{"] _2 KW["}"],
   Validate.1:opt                             -- _1,
   Extension                                  -- _1 KW["{"] _2 KW["}"],
   Extension.1:iter                           -- _1,
   Extension.2:opt                            -- _1,
   Root                                       -- KW["/"],
   RootPath                                   -- KW["/"] _1,
   RootPath2                                  -- KW["//"] _1,
   UnaryMinus                                 -- KW["-"] _1,
   UnaryPlus                                  -- KW["+"] _1,
   Cast                                       -- _1 _2,
   Cast.2:seq                                 -- KW["cast"] KW["as"] _1,
   Castable                                   -- _1 _2,
   Castable.2:seq                             -- KW["castable"] KW["as"] _1,
   Treat                                      -- _1 _2,
   Treat.2:seq                                -- KW["treat"] KW["as"] _1,
   InstanceOf                                 -- _1 _2,
   InstanceOf.2:seq                           -- H hs=1 [KW["instance"] KW["of"] _1],
   Intersect                                  -- H hs=1 [_1 KW["intersect"] _2],
   Except                                     -- H hs=1 [_1 KW["except"] _2],
   Union                                      -- H hs=1 [_1 KW["union"] _2],
   Union                                      -- H hs=1 [_1 KW["|"] _2],
   Mul                                        -- H hs=1 [_1 KW["*"] _2],
   Div                                        -- H hs=1 [_1 KW["div"] _2],
   IDiv                                       -- H hs=1 [_1 KW["idiv"] _2],
   Mod                                        -- H hs=1 [_1 KW["mod"] _2],
   Add                                        -- H hs=1 [_1 KW["+"] _2],
   Sub                                        -- H hs=1 [_1 KW["-"] _2],
   Range                                      -- H hs=1 [_1 KW["to"] _2],
   GeneralEq                                  -- H hs=1 [_1 KW["="] _2],
   GeneralNe                                  -- H hs=1 [_1 KW["!="] _2],
   GeneralLt                                  -- H hs=1 [_1 KW["<"] _2],
   GeneralLe                                  -- H hs=1 [_1 KW["<="] _2],
   GeneralGt                                  -- H hs=1 [_1 KW[">"] _2],
   GeneralGe                                  -- H hs=1 [_1 KW[">="] _2],
   ValueEq                                    -- H hs=1 [_1 KW["eq"] _2],
   ValueNe                                    -- H hs=1 [_1 KW["ne"] _2],
   ValueLt                                    -- H hs=1 [_1 KW["lt"] _2],
   ValueLe                                    -- H hs=1 [_1 KW["le"] _2],
   ValueGt                                    -- H hs=1 [_1 KW["gt"] _2],
   ValueGe                                    -- H hs=1 [_1 KW["ge"] _2],
   Is                                         -- H hs=1 [_1 KW["is"] _2],
   Precedes                                   -- H hs=1 [_1 KW["<<"] _2],
   Follows                                    -- H hs=1 [_1 KW[">>"] _2],
   And                                        -- H hs=1 [_1 KW["and"] _2],
   Or                                         -- H hs=1 [_1 KW["or"] _2],
   FLWOR                                      -- V is=2 [V[_1 _2 _3] H[KW["return"] _4]],
   FLWOR.1:iter                               -- _1,
   FLWOR.1:iter.1:alt                         -- _1 _2,
   FLWOR.2:opt                                -- _1,
   FLWOR.3:opt                                -- _1,
   Some                                       -- H hs=1 [KW["some"] _1 KW["satisfies"] _2],
   Some.1:iter-sep                            -- _1 KW[","],
   Some.1:iter-sep.1:seq                      -- H hs=1 [H hs=0 [KW["$"] _1] _2 KW["in"] _3],
   Some.1:iter-sep.1:seq.2:opt                -- _1,
   Every                                      -- H hs=1 [KW["every"] _1 KW["satisfies"] _2],
   Every.1:iter-sep                           -- _1 KW[","],
   Every.1:iter-sep.1:seq                     -- H hs=1 [H hs=0 [KW["$"] _1] _2 KW["in"] _3],
   Every.1:iter-sep.1:seq.2:opt               -- _1,
   TypeSwitch                                 -- H hs=1 [KW["typeswitch"] KW["("] _1 KW[")"] _2 KW["default"] _3 KW["return"] _4],
   TypeSwitch.2:iter                          -- _1,
   TypeSwitch.3:opt                           -- _1,
   TypeSwitch.3:opt.1:seq                     -- KW["$"] _1,
   If                                         -- V[V is=2 [H[KW["if"] H hs=0 [KW["("] _1 KW[")"]] KW["then"]] _2] V is=2 [KW["else"] _3]],
   Comma                                      -- H hs=1 [H hs=0 [_1 KW[","]] _2],
   Comma                                      -- H hs=1 [H hs=0 [_1 KW[","]] _2],
   DirElemConstructor1                        -- H hs=0 [KW["<"] H hs=1 [_1 _2] KW["/>"]],
   DirElemConstructor2                        -- V[V is=2 [H hs=0 [KW["<"] H hs=1 [_1 _2] KW[">"]] _3] H hs=0 [KW["</"] _4 KW[">"]]],
   DirElemContents                            -- _1,
   DirElemContents.1:iter-star                -- _1,
   DirAttributes                              -- _1,
   DirAttributes.1:iter-star                  -- _1,
   DirAttribute                               -- H hs=0 [_1 KW["="] KW["\""] _2 KW["\""]],
   DirAttribute                               -- H hs=0 [_1 KW["="] KW["'"] _2 KW["'"]],
   AttrValues                                 -- _1,
   AttrValues.1:iter-star                     -- _1,
   AttrValues.1:iter-star.1:alt               -- _1 _2,
   AttrString                                 -- _1,
   Text                                       -- _1,
   LCurly                                     -- KW["{{"],
   RCurly                                     -- KW["}}"],
   CompDoc                                    -- KW["document"] KW["{"] _1 KW["}"],
   CompElem1                                  -- KW["element"] _1 KW["{"] _2 KW["}"],
   CompElem1.2:opt                            -- _1,
   CompElem2                                  -- KW["element"] KW["{"] _1 KW["}"] KW["{"] _2 KW["}"],
   CompElem2.2:opt                            -- _1,
   CompAttr1                                  -- KW["attribute"] _1 KW["{"] _2 KW["}"],
   CompAttr1.2:opt                            -- _1,
   CompAttr2                                  -- KW["attribute"] KW["{"] _1 KW["}"] KW["{"] _2 KW["}"],
   CompAttr2.2:opt                            -- _1,
   CompText                                   -- KW["text"] KW["{"] _1 KW["}"],
   CompComment                                -- KW["comment"] KW["{"] _1 KW["}"],
   CompPI                                     -- KW["processing-instruction"] _1 KW["{"] _2 KW["}"],
   CompPI.1:alt                               -- _1 _2,
   CompPI.1:alt.2:seq                         -- KW["{"] _1 KW["}"],
   CompPI.2:opt                               -- _1,
   SingleType                                 -- _1,
   SingleTypeQ                                -- _1 KW["?"],
   TypeDeclaration                            -- KW["as"] _1,
   EmptySeqType                               -- KW["empty-sequence"] KW["("] KW[")"],
   OccursType                                 -- _1 _2,
   ItemTypeQ                                  -- _1,
   ItemTypeQ.1:alt                            -- KW["?"] KW["*"] KW["+"],
   Item                                       -- H hs=0 [KW["item"] KW["("] KW[")"]],
   AxisChild                                  -- H hs=0 [KW["child"] KW["::"]],
   AxisDescendant                             -- H hs=0 [KW["descendant"] KW["::"]],
   AxisAttribute                              -- H hs=0 [KW["attribute"] KW["::"]],
   AxisSelf                                   -- H hs=0 [KW["self"] KW["::"]],
   AxisDescendantOrSelf                       -- H hs=0 [KW["descendant-or-self"] KW["::"]],
   AxisFollowingSibling                       -- H hs=0 [KW["following-sibling"] KW["::"]],
   AxisFollowing                              -- H hs=0 [KW["following"] KW["::"]],
   AxisParent                                 -- H hs=0 [KW["parent"] KW["::"]],
   AxisAncestor                               -- H hs=0 [KW["ancestor"] KW["::"]],
   AxisPrecedingSibling                       -- H hs=0 [KW["preceding-sibling"] KW["::"]],
   AxisPreceding                              -- H hs=0 [KW["preceding"] KW["::"]],
   AxisAncestorOrSelf                         -- H hs=0 [KW["ancestor-or-self"] KW["::"]],
   NameTest                                   -- _1,
   NameTest                                   -- _1,
   Wildcard1                             -- KW["*"],
   Wildcard2                             -- _1 KW[":"] KW["*"],
   Wildcard3                             -- KW["*"] KW[":"] _1,
   VarName                                    -- _1,
   AnyKindTest                                -- H hs=0 [KW["node"] KW["("] KW[")"]],
   DocumentTest                               -- H hs=0 [KW["document-node"] KW["("] H [_1] KW[")"]],
   DocumentTest.1:opt                         -- _1,
   DocumentTest.1:opt.1:alt                   -- _1 _2,
   TextTest                                   -- H hs=0 [KW["text"] KW["("] KW[")"]],
   CommentTest                                -- H hs=0 [KW["comment"] KW["("] KW[")"]],
   PITest                                     -- H hs=0 [KW["processing-instruction"] KW["("] H[_1] KW[")"]],
   PITest.1:opt                               -- _1,
   PITest.1:opt.1:alt                         -- _1 _2,
   AttributeTest                              -- H hs=0 [KW["attribute"] KW["("] H[_1] KW[")"]],
   AttributeTest.1:opt                        -- _1,
   AttributeTest.1:opt.1:seq                  -- _1 _2,
   AttributeTest.1:opt.1:seq.2:opt            -- _1,
   AttributeTest.1:opt.1:seq.2:opt.1:seq      -- KW[","] _1,
   AttribName                                 -- _1,
   Wildcard                                   -- KW["*"],
   SchemaAttributeTest                        -- H hs=0 [KW["schema-attribute"] KW["("] H[_1] KW[")"]],
   ElementTest                                -- H hs=0 [KW["element"] KW["("] _1 KW[")"]],
   ElementTest.1:opt                          -- _1,
   Opt1                                       -- _1 _2,
   Opt1.2:opt                                 -- _1,
   Opt2                                       -- KW[","] _1 _2,
   Opt2.2:opt                                 -- _1,
   Opt3                                       -- KW["?"],
   ElementName                                -- _1,
   Wildcard                                   -- KW["*"],
   SchemaElementTest                          -- H hs=0 [KW["schema-element"] KW["("] _1 KW[")"]],
   Chars                                      -- H hs=0 [KW["\""] _1 KW["\""]],
   Chars                                      -- H hs=0 [KW["'"] _1 KW["'"]],
   UQName                                     -- _1,
   PQName                                     -- H hs=0 [_1 KW[":"] _2],
   CDataSection                               -- KW["xxxxxxxxxxxxxxxxxxxxxxxxxxx"],
   DirCommentConstructor                      -- KW["xxxxxxxxxxxxxxxxxxxxxxxxxxx"],
   DirPIConstructor                           -- KW["xxxxxxxxxxxxxxxxxxxxxxxxxxx"],
   PragmaContents                             -- KW["xxxxxxxxxxxxxxxxxxxxxxxxxxx"],
   Parenthetical                              -- H hs=0 ["(" _1 ")"]
]
