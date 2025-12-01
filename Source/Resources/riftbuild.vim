" RiftBuild syntax highlighting

if exists("b:current_syntax")
  finish
endif

highlight riftbuildKeywordColor   guifg=#FFD700 ctermfg=Yellow
highlight riftbuildKeyColor       guifg=#90f1c4 ctermfg=37
highlight riftbuildIfElseColor    guifg=#FF5020 ctermfg=Red
highlight riftbuildRefSymbolColor guifg=#C586C0 ctermfg=Magenta
highlight riftbuildBraceColor     guifg=#808080 ctermfg=Grey

" -------------------------------
" Braces / brackets / parentheses
" -------------------------------
syntax match riftbuildBraces "[{}()\[\]]"
highlight link riftbuildBraces riftbuildBraceColor

" -------------------------------
" Symbols
" -------------------------------
syntax match riftbuildConditionals "[|!^]"
highlight riftbuildConditionals guifg=#ce5ae8 ctermfg=Magenta

syntax match riftbuildVarReferenceSymbols  "[%@$][^-]\(\([A-Za-z0-9_.]\+\)\|(\([A-Za-z0-9_.]\+\))\|{\([A-Za-z0-9_.]\+\)}\)\?"
highlight link riftbuildVarReferenceSymbols riftbuildRefSymbolColor

" -------------------------------
" Keys (allow dots inside keys)
" -------------------------------
syntax match riftbuildKey "^\s*[A-Za-z0-9_.]*\s*"
highlight link riftbuildKey riftbuildKeyColor

" -------------------------------
" Special keywords
" -------------------------------
syntax match riftbuildSpecial "^\s*\.\S*"
highlight link riftbuildSpecial riftbuildKeywordColor

syntax match riftbuildSpecial ".*.ErrorMessage"
highlight link riftbuildSpecial riftbuildKeywordColor

syntax match riftbuildDepend "^\s*\cDepend[s]*"
highlight link riftbuildDepend riftbuildKeywordColor

syntax match riftbuildInclude "^\s*\c\<Include\>"
highlight link riftbuildInclude riftbuildKeywordColor

syntax match riftbuildBuildCmd "^\s*\c\(\<PreBuild\>\|\<PostBuild\>\|\<PreCompile\>\|\<PostCompile\>\|\<PreLink\>\|\<PostLink\>\)\S*"
highlight link riftbuildBuildCmd riftbuildKeywordColor

syntax match riftbuildOptionKey "^\s*\cOption.[A-Za-z0-9_.]*\s*"
highlight link riftbuildOptionKey riftbuildKeywordColor

" -------------------------------
" if/else keyword
" -------------------------------
syntax match riftbuildIf "^\s*\<if\>"
highlight link riftbuildIf riftbuildIfElseColor

syntax match riftbuildElse "^\s*\<else\>"
highlight link riftbuildElse riftbuildIfElseColor

" -------------------------------
" Quoted strings
" -------------------------------
syntax match riftbuildString /".*"/ contains=riftbuildVarReferenceSymbols
highlight riftbuildString guifg=#CE9178 ctermfg=214

" -------------------------------
" Values
" -------------------------------
" Technically not needed
"syntax region riftbuildValue start="^\s*[A-Za-z_][A-Za-z0-9_.]*\s\+\zs" end="$"
"highlight riftbuildValue guifg=#FFFFFF ctermfg=15

" -------------------------------
" Disable highlighting in .Help {} and .ErrorMessage {} blocks
" -------------------------------
syntax region riftbuildHelpBlock start="\c.Help\s*$" end="}\s*$" keepend contains=riftbuildSpecial,riftbuildVarReferenceSymbols
syntax region riftbuildErrorBlock start=".*.ErrorMessage" end="}\s*$" keepend contains=riftbuildSpecial,riftbuildVarReferenceSymbols

" -------------------------------
" Comments
" -------------------------------
syntax match riftbuildComment "#.*$" containedin=ALL
syntax region riftbuildMultiLineComment start="##\s*$" end="##\s*$" keepend containedin=ALL
highlight riftbuildComment guifg=#6A9955 ctermfg=65
highlight riftbuildMultiLineComment guifg=#6A9955 ctermfg=65

let b:current_syntax = "riftbuild"

