" RiftBuild syntax highlighting
" Matches the syntax accepted by Source/Parse.c

if exists("b:current_syntax")
  finish
endif

syntax case ignore

highlight riftbuildKeywordColor     guifg=#FFD700 ctermfg=Yellow
highlight riftbuildKeyColor         guifg=#90f1c4 ctermfg=37
highlight riftbuildReservedKeyColor guifg=#4EC9B0 ctermfg=43
highlight riftbuildIfElseColor      guifg=#FF5020 ctermfg=Red
highlight riftbuildRefSymbolColor   guifg=#C586C0 ctermfg=Magenta
highlight riftbuildBraceColor       guifg=#808080 ctermfg=Grey

" -------------------------------
" Braces / brackets / parentheses
" -------------------------------
syntax match riftbuildBraces "[{}()\[\]]"
highlight link riftbuildBraces riftbuildBraceColor

" -------------------------------
" Operators and symbols
" -------------------------------
" comparison / condition symbols: == != <= >= < > = ! | and the
" case-sensitivity marker ^, statement separator ;, backtick value reset
syntax match riftbuildConditionals "==\|!=\|<=\|>=\|[<>=!|^;`]"
highlight riftbuildConditionals guifg=#ce5ae8 ctermfg=Magenta

" version comparison operators: v== v> v< v>= v<=
syntax match riftbuildVersionOps "\<v\(==\|>=\|<=\|>\|<\)"
highlight link riftbuildVersionOps riftbuildConditionals

" variable references: %Name $Name @Name, %{Name} %(Name), %-(Name)
syntax match riftbuildVarReferenceSymbols "[%$@]-\?\({[A-Za-z0-9_.-]\+}\|([A-Za-z0-9_.-]\+)\|[A-Za-z0-9_.]\+\)\?"
highlight link riftbuildVarReferenceSymbols riftbuildRefSymbolColor

" built-in variables: %_DirectoryName, %_Date.Year, %_Platform, ...
syntax match riftbuildBuiltinVar "[%$@]_[A-Za-z0-9_.]*"
highlight riftbuildBuiltinVar guifg=#DA70D6 ctermfg=170

" -------------------------------
" Keys (allow dots inside keys)
" -------------------------------
syntax match riftbuildKey "^\s*[A-Za-z_][A-Za-z0-9_.]*"
highlight link riftbuildKey riftbuildKeyColor

" reserved keys get their own color
syntax match riftbuildReservedKey "^\s*\(Assembly\|Extension\|Type\|SourceFiles\|SourceDirectory\|SourceDirectories\|BuildDirectory\|IntermediateDirectory\|Compiler\|Linker\|Assembler\|Archiver\|Libraries\|Library\|Apple\|Icon\|PCH\|Bundle\|Info\.plist\|Version\.plist\|TitleName\|InternalName\|Description\|CompanyName\|Copyright\|Version\|License\|AlwaysRebuild\|AlwaysRebuildAll\)\(\.[A-Za-z0-9_.]\+\)\?\([:( \t`]\|$\)\@="
highlight link riftbuildReservedKey riftbuildReservedKeyColor

" -------------------------------
" Special keywords
" -------------------------------
" directives: .Run .Stop .Abort .Help
syntax match riftbuildSpecial "^\s*\.\(run\|stop\|abort\|help\)\>"
highlight link riftbuildSpecial riftbuildKeywordColor

" any key ending in .ErrorMessage
syntax match riftbuildErrorMessageKey "[A-Za-z0-9_.]*\.ErrorMessage\>"
highlight link riftbuildErrorMessageKey riftbuildKeywordColor

syntax match riftbuildDepend "^\s*Depends\?\>"
highlight link riftbuildDepend riftbuildKeywordColor

syntax match riftbuildInclude "\<\(include\|import\)\>"
highlight link riftbuildInclude riftbuildKeywordColor

" build phase hooks and their file-operation verbs
syntax match riftbuildBuildCmd "^\s*\(Pre\|Post\)\(Depend\|Build\|Compile\(AllFiles\|File\)\?\|Link\)\(\.\(Cmd\|Copy\|NewDir\|NewFile\|WriteFile\|AppendFile\|Rename\|Delete\)\)\?\>"
highlight link riftbuildBuildCmd riftbuildKeywordColor

" options and presets
syntax match riftbuildOptionKey "^\s*option\.[A-Za-z0-9_.]*"
highlight link riftbuildOptionKey riftbuildKeywordColor

syntax match riftbuildPresetKey "^\s*\(preset\|default\.options\)[.:]\@="
highlight link riftbuildPresetKey riftbuildKeywordColor

syntax match riftbuildAssert "^\s*Assert\.[A-Za-z0-9_.]*"
highlight link riftbuildAssert riftbuildKeywordColor

" -------------------------------
" Keywords
" -------------------------------
syntax match riftbuildIf "\<if\>"
highlight link riftbuildIf riftbuildIfElseColor

syntax match riftbuildElse "\<else\>"
highlight link riftbuildElse riftbuildIfElseColor

" condition keywords
syntax match riftbuildCondKeyword "\<\(or\|contains\|starts_with\|ends_with\)\>"
highlight link riftbuildCondKeyword riftbuildIfElseColor

" -------------------------------
" Quoted strings
" -------------------------------
" \" and \# are escaped characters - they do not start strings or comments
syntax match riftbuildEscape "\\[\"#]"
highlight link riftbuildEscape riftbuildRefSymbolColor

" a string starts at a quote NOT preceded by a backslash and may contain
" escaped quotes
syntax match riftbuildString /\\\@<!"\%(\\.\|[^"]\)*"/ contains=riftbuildEscape,riftbuildBuiltinVar,riftbuildVarReferenceSymbols
highlight riftbuildString guifg=#CE9178 ctermfg=214

" -------------------------------
" .Help / .ErrorMessage / WriteFile heredoc blocks - contents are plain text
" -------------------------------
syntax region riftbuildHelpBlock start="^\s*\.help\s*{\?\s*$" end="^\s*}" keepend contains=riftbuildSpecial,riftbuildBuiltinVar,riftbuildVarReferenceSymbols
syntax region riftbuildErrorBlock start="^\s*[A-Za-z0-9_.]*\.ErrorMessage\s*{\?\s*$" end="^\s*}" keepend contains=riftbuildErrorMessageKey,riftbuildBuiltinVar,riftbuildVarReferenceSymbols
syntax region riftbuildHeredocBlock start="^\s*\(Pre\|Post\)\(Depend\|Build\|Compile\|Link\)\.\(WriteFile\|AppendFile\)\>[^{#]*{\?\s*$" end="^\s*}" keepend contains=riftbuildBuildCmd,riftbuildBuiltinVar,riftbuildVarReferenceSymbols

" -------------------------------
" Value blocks - [ ... ]
" -------------------------------
syntax region riftbuildMultiLineValue start="^\s*\[\s*$" end="\]\s*$" keepend contains=riftbuildBuiltinVar,riftbuildVarReferenceSymbols,riftbuildComment,riftbuildMultiLineComment

" -------------------------------
" Comments - # to end of line, ## ... ## block, \# is escaped
" -------------------------------
syntax match riftbuildComment "\\\@<!#.*$" containedin=ALL
syntax region riftbuildMultiLineComment start="\\\@<!##" end="##" keepend containedin=ALL
highlight riftbuildComment guifg=#6A9955 ctermfg=65
highlight riftbuildMultiLineComment guifg=#6A9955 ctermfg=65

let b:current_syntax = "riftbuild"
