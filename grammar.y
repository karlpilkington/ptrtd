
%{
#include <errno.h>
#include <string.h>
#include <stdio.h>
#define YYERROR_VERBOSE
#include "scanner.h"
#include "defs.h"
static void unknown_symbol(char const * s, int lineno);
static char const * filename;
%}

%union {
    unsigned short p[4];
    char s[64];
    int d;
}

%token <d> INT
%token <d> PLEN
%token <s> ID
%token <p> PREFIX

%start settings

%%

settings : /* nothing */ | setting settings
        {
            printf("%s successfully parsed.\n", filename);
            YYACCEPT;
        };


setting : ID '=' PREFIX PLEN ';'
        {
            if(strcmp($1, "prefix") == 0){
                int i;
                printf("using %s: %x:%x:%x:%x::/%d \n", $1, ($3)[0], ($3)[1], ($3)[2], ($3)[3], $4);
                for(i=0; i < 4; ++i)
                    globals.prefix[i] = htons($3[i]);
                globals.plen = $4;
            }
            else {
                unknown_symbol($1, yylineno);
            }
        }
        | ID '=' INT ';'
        {
            if(strcmp($1, "http_port") == 0){
                printf("using %s: %d \n", $1, $3);
                globals.http_port = $3;
            }
            else {
                unknown_symbol($1, yylineno);
            }
        }

%%

static void unknown_symbol(char const * s, int lineno)
{
    fprintf(stderr, "Symantic Error! Unrecognized ID: \"%s\" on line %d in %s\n", s, lineno, filename);
    fprintf(stderr, "Please fix the above error(s) and retry.\n");
    exit(1);
}

int yyerror(char * s)
{
    fprintf(stderr, "PARSE ERROR! on line %d\n", yylineno);
    return 0;
}

int read_config(char const * fn)
{
    filename = fn;
    yyin = fopen(fn, "r");
    if(!yyin){
        printf("unable to open %s, %s\n", fn, strerror(errno));
        return 0;
    }

    if(yyparse() != 0)
    {
        printf("Unable to parse config file %s\n", fn);
        return 0;
    }

    return 1;
}
