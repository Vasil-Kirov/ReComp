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

syn match rcpSIdentifier  "\%([^[:cntrl:][:space:][:punct:][:digit:]]\|_\)\%([^[:cntrl:][:punct:][:space:]]\|_\)*" display contained
syn match rcpSDecl /\w\+\s*:/ contains=rcpSIdentifier
syn match rcpSFuncCall "\w\(\w\)*("he=e-1,me=e-1

" Strings
syn region rcpSString      start=+c"+ skip=+\\\\\|\\"+ end=+"+
syn region rcpSString      start=+"+ skip=+\\\\\|\\"+ end=+"+

" Strings
syn region rcpSChar      start=+c'+ skip=+\\\\\|\\'+ end=+'+
syn region rcpSChar      start=+'+ skip=+\\\\\|\\'+ end=+'+

" Brackets and punctuation
syn match rcpSBracket "[{}()\[\]]"
syn match rcpSOperator "[-+=*/<>:&]"

" Keywords
syn keyword rcpSKeyword union enum struct fn return for in if else match break defer type_info type_of size_of as continue using yield then cast bit_cast module
syn keyword rcpSConstant null true false inf nan
syn match rcpSImport /#import/
syn match rcpSCompilerDir /#foreign/
syn match rcpSCompilerDir /#inline/
syn match rcpSCompilerDir /#link/
syn match rcpSCompilerDir /#private/
syn match rcpSCompilerDir /#public/
syn match rcpSCompilerDir /#if/
syn match rcpSCompilerDir /#elif/
syn match rcpSCompilerDir /#else/
syn match rcpSCompilerDir /#assert/
syn match rcpSCompilerDir /#run/
syn match rcpSCompilerDir /#embed_str/
syn match rcpSCompilerDir /#embed_bin/
syn match rcpSCompilerDir /#load_system_dl/
syn match rcpSCompilerDir /#load_dl/
syn match rcpSCompilerDir /#intrinsic/

" Types
syn keyword rcpSType f64 f32 i64 i32 i16 i8 u64 u32 u16 u8 int uint string type bool void v2 v4 iv2 iv4
syn match rcpSPtrStar /\*/
syn match rcpSTypeCast /@/ nextgroup=rcpSType,rcpSPtrStar skipwhite


" Number literals
syn match rcpSNumber "\<[0-9_]\+\>"
syn match rcpSBinaryNumber /\<0[bB][01_]\+\>/
syn match rcpSHexNumber /\<0[xX][0-9a-fA-F_]\+\>/

" Comments (starting with //)
syn match rcpSTodo "@\<\w\+\>" contained display
syn match rcpSComment "//.*$" contains=rcpSTodo
syn region rcpSBlockComment start=/\v\/\*/ end=/\v\*\// contains=rcpSBlockComment, rcpSTodo


" Highlighting groups
hi def link rcpSCompilerDir PreProc
hi def link rcpSImport Include
hi def link rcpSNumber Number
hi def link rcpSBinaryNumber Number
hi def link rcpSHexNumber Number
hi def link rcpSIdentifier Identifier
hi def link rcpSConstant Constant
hi def link rcpSKeyword Keyword
hi def link rcpSType Type
hi def link rcpSTodo Todo
hi def link rcpSComment Comment
hi def link rcpSBlockComment Comment
hi def link rcpSString String
hi def link rcpSChar SpecialChar
hi def link rcpSFuncCall Function
" 
hi def link rcpSOperator Operator
hi def link rcpSBracket Delimiter
hi def link rcpSTypeCast Special

let b:current_syntax = "rcpS"
