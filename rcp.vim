" Custom Language Syntax Highlighting for Vim
" Place this file in ~/.vim/syntax/ and use :set syntax=rcpS to activate

if exists("b:current_syntax")
  finish
endif

function s:comment()

	syn match rcpSFunction "\w+\s*:\s*\w*:\s*fn"

	hi def link rcpSFunction Function

	syn match rcpSString "c?\"(?:[^\"\\]|\\.)*\""
endfunction

syn match rcpSVariable contained /\w\+/
syn match rcpSDecl /\w\+\s*:/ contains=rcpSVariable

" Strings
syn region rcpSString      start=+c"+ skip=+\\\\\|\\"+ end=+"+
syn region rcpSString      start=+"+ skip=+\\\\\|\\"+ end=+"+

" Brackets and punctuation
syn match rcpSBracket "[{}()\[\]]"
syn match rcpSOperator "[-+=*/<>:&]"

" Keywords
syn keyword rcpSKeyword union enum struct fn return for in if else match break defer type_of size_of as continue
syn keyword rcpSConstant null true false
syn match rcpSImport /#import/
syn match rcpSCompilerDir /#foreign/
syn match rcpSCompilerDir /#link/
syn match rcpSCompilerDir /#info/
syn match rcpSCompilerDir /#private/
syn match rcpSCompilerDir /#public/

" Types
syn keyword rcpSType f64 f32 i64 i32 i16 i8 u64 u32 u16 u8 int uint string type bool void
syn match rcpSPtrStar /\*/
syn match rcpSTypeCast /@/ nextgroup=rcpSType,rcpSPtrStar skipwhite


" Number literals
syn match rcpSNumber "\<[0-9_]\+\>"
syn match rcpSBinaryNumber /\<0[bB][01_]\+\>/
syn match rcpSHexNumber /\<0[xX][0-9a-fA-F_]\+\>/

" Comments (starting with //)
syn keyword rcpSTodo contained TODO FIXME LEAK XXX NOTE
syn match rcpSComment "//.*$" contains=rcpSTodo


" Highlighting groups
hi def link rcpSCompilerDir PreProc
hi def link rcpSImport Include
hi def link rcpSNumber Number
hi def link rcpSBinaryNumber Number
hi def link rcpSHexNumber Number
hi def link rcpSVariable Identifier
hi def link rcpSConstant Constant
hi def link rcpSKeyword Keyword
hi def link rcpSType Type
hi def link rcpSTodo Todo
hi def link rcpSComment Comment
hi def link rcpSString String
" 
hi def link rcpSOperator Operator
hi def link rcpSBracket Delimiter
hi def link rcpSTypeCast Special

let b:current_syntax = "rcpS"
