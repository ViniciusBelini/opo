# Language Grammar

The Opo Programming Language uses a unique symbolic grammar designed for clarity and efficiency. Below is a simplified representation of the Opo grammar in EBNF-like notation.

## Core Structure

```ebnf
program      = { declaration } ;

declaration  = funDecl | structDecl | enumDecl | impDecl | statement ;

funDecl      = [ "pub" ] "<" [ params ] ">" "->" type ":" identifier block ;
structDecl   = [ "pub" ] "struct" "[" [ fieldDecls ] "]" "=>" identifier ":" "type" ;
enumDecl     = [ "pub" ] "enum" "[" [ variantDecls ] "]" "=>" identifier ":" "type" ;
impDecl      = string "=>" identifier ":" "imp" ;
```

## Statements

```ebnf
statement    = exprStmt | assignStmt | ifStmt | whileStmt | matchStmt | returnStmt | controlStmt | printStmt ;

exprStmt     = expression ;
assignStmt   = expression "=>" identifier [ ":" type ] ;
ifStmt       = expression "?" block ":" block ;
whileStmt    = expression "@" block ;
matchStmt    = "match" expression "[" { matchCase } "]" ;
returnStmt   = "^" [ expression ] ;
controlStmt  = "." | ".." ;  (* break and continue *)
printStmt    = expression "!!" ;
block        = "[" { statement } "]" ;
```

## Expressions

```ebnf
expression   = assignment | logic_or ;
logic_or     = logic_and { "||" logic_and } ;
logic_and    = equality { "&&" equality } ;
equality     = comparison { ( "==" | "!=" ) comparison } ;
comparison   = term { ( ">" | ">=" | "<" | "<=" ) term } ;
term         = factor { ( "+" | "-" ) factor } ;
factor       = unary { ( "*" | "/" | "%" ) unary } ;
unary        = ( "!" | "-" ) unary | call ;
call         = primary { "(" [ arguments ] ")" | "." identifier | "." integer } ;
primary      = identifier | number | string | "tru" | "fls" | "none" | "some" "(" expression ")" | "(" expression ")" | anonFun ;
anonFun      = "<" [ params ] ">" "->" type block ;
```

## Types and Parameters

```ebnf
type         = "int" | "flt" | "bol" | "str" | "void" | "any" | identifier | arrayType | mapType | optionType | resultType ;
arrayType    = "[]" type ;
mapType      = "{" type ":" type "}" ;
optionType   = type "?" ;
resultType   = type "!" ;

params       = identifier ":" type { "," identifier ":" type } ;
arguments    = expression { "," expression } ;
fieldDecls   = identifier ":" type { "," identifier ":" type } ;
variantDecls = identifier [ "(" type ")" ] { "," identifier [ "(" type ")" ] } ;
matchCase    = identifier [ "(" identifier ")" ] block ;
```

## Lexical Elements

- **Identifier**: `[a-zA-Z_][a-zA-Z0-9_]*`
- **Integer**: `[0-9]+`
- **Float**: `[0-9]+\.[0-9]+`
- **String**: `\"[^\"]*\"`
- **Comments**: `#` until the end of the line.
