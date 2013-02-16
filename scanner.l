
%{
#include <string.h>
#include <stdlib.h>
#include "grammar.h"
%}

%option noyywrap yylineno header="../scanner.h"
%x COMMENT

%%

([0-9a-fA-F]{1,4}:){1,4}:               {unsigned short * p = yylval.p;
                                         memset(p, 0, 4 * sizeof(unsigned short));
                                         sscanf(yytext, "%hx:%hx:%hx:%hx:", p, p+1, p+2, p+3); 
                                         return PREFIX;}

\/[0-9]{2}                              {yylval.d = atol(yytext+1); return PLEN;}
[0-9]+                                  {yylval.d = atol(yytext); return INT;}
"true"|"yes"|"on"                       {yylval.d = 1; return INT;};
"false"|"no"|"off"                      {yylval.d = 0; return INT;};
[_a-zA-Z][_a-zA-Z0-9]*                  {strncpy(yylval.s, yytext, 64); return ID;}
[=;(),]                                 return yytext[0];
[ \t\n]                                 ;
#                                       BEGIN COMMENT;
<COMMENT>\n                             BEGIN INITIAL;
<COMMENT>.                              ;
.                                       return yytext[0];

%%

