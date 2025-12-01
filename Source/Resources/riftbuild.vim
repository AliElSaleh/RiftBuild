" RiftBuild syntax highlighting

if exists("b:current_syntax")
  finish
endif

" -------------------------------
" Comments
" -------------------------------
syntax match riftbuildComment "^#.*$" containedin=ALL
syntax region riftbuildMultiLineComment start="^\s*##" end="^\s*##\s*$" keepend contains=NONE
highlight riftbuildComment guifg=#6A9955 ctermfg=65
highlight riftbuildMultiLineComment guifg=#6A9955 ctermfg=65

" -------------------------------
" Special keywords
" -------------------------------
syntax match riftbuildSpecial "^\.\S*" containedin=ALL
highlight riftbuildSpecial guifg=#FFD700 ctermfg=Yellow

syntax match riftbuildReserved "^\.\(Assert\|Option\|PreBuild\)" containedin=ALL
highlight riftbuildReserved guifg=#FFD700 ctermfg=Yellow

syntax match riftbuildDepend "Depend" containedin=ALL
highlight riftbuildDepend guifg=#FFD700 ctermfg=Yellow

" -------------------------------
" Braces / brackets / parentheses
" -------------------------------
syntax match riftbuildBraces "[{}()\[\]]" containedin=ALL
highlight riftbuildBraces guifg=#D4D4D4 ctermfg=188

" -------------------------------
" If keyword and blocks
" -------------------------------
syntax keyword riftbuildIf if containedin=ALL
highlight riftbuildIf guifg=#569CD6 ctermfg=33

" -------------------------------
" Keys (allow dots inside keys)
" -------------------------------
syntax match riftbuildKey "^\s*[A-Za-z_][A-Za-z0-9_.]*\s*" containedin=ALL
highlight riftbuildKey guifg=#4EC9B0 ctermfg=37

" -------------------------------
" Values
" -------------------------------
syntax region riftbuildValue start="^\s*[A-Za-z_][A-Za-z0-9_.]*\s\+\zs" end="$" contains=riftbuildPercent,riftbuildDollar,riftbuildAt,riftbuildUpper,riftbuildLower,riftbuildOperator,riftbuildSpecial containedin=ALL
highlight riftbuildValue guifg=#FFFFFF ctermfg=15

" -------------------------------
" Tokens inside values
" -------------------------------
syntax match riftbuildPercent "%\w\+" contained
syntax match riftbuildDollar "\$\w\+" contained
syntax match riftbuildAt "@\w\+" contained
syntax match riftbuildUpper "\<[A-Z]\w*\>" contained
syntax match riftbuildLower "\<[a-z]\w*\>" contained
syntax match riftbuildOperator "[!^:_]" contained

highlight riftbuildPercent guifg=#C586C0 ctermfg=132
highlight riftbuildDollar guifg=#B5CEA8 ctermfg=114
highlight riftbuildAt guifg=#4EC9B0 ctermfg=37
highlight riftbuildUpper guifg=#DCDCAA ctermfg=185
highlight riftbuildLower guifg=#9CDCFE ctermfg=81
highlight riftbuildOperator guifg=#D4D4D4 ctermfg=188

" -------------------------------
" Include keyword
" -------------------------------
syntax keyword riftbuildInclude include containedin=ALL
highlight riftbuildInclude guifg=#569CD6 ctermfg=33

" -------------------------------
" Disable highlighting in .Help {} and .ErrorMessage {} blocks
" -------------------------------
syntax region riftbuildNoHighlight start="^\s*\.Help\s*{" end="}" contains=NONE
syntax region riftbuildNoHighlight start="^\s*\.ErrorMessage\s*{" end="}" contains=NONE

let b:current_syntax = "riftbuild"

