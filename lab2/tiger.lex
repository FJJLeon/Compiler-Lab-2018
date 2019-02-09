%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "tokens.h"
#include "errormsg.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

/* @function: getstr
 * @input: a string literal
 * @output: the string value for the input which has all the escape sequences 
 * translated into their meaning.
 */

char *getstr(const char *str)
{
	//optional: implement this function if you need it
	char * result;
	int len = strlen(str);
	//printf("str %s, len %d\n", str, len);
	if (len-2 == 0) {// null string
        result = (char *)checked_malloc(7);
        strncpy(result, "(null)", 6); // a null string must return  (null)\0
        return result;
    }

	result = (char *)malloc(yyleng + 1);
	size_t cur = 0, i = 1; // skip double quotes
	while (i < len-1) {
		if (str[i] == '\\' ) {
			switch(str[i+1]) {
				case '"': result[cur] = '"'; break;
                case 'n': result[cur] = '\n'; break;
                case 't': result[cur] = '\t'; break;
                case '\\': result[cur] = '\\'; break;
                case '\n': 
                {// no actual string, jump to after the next \ pos
					size_t next = strchr(str+i+1, '\\') - str;
					cur--;		   // no result added
					i = next - 1;  // note final += 2 
					break;
				}
                case '^':
                {// control character
                	i++; // need +3
                	switch (str[i+1]) {
                		case 'C': result[cur] = '\x03'; break;
                		case 'O': result[cur] = '\x0F'; break;
                		case 'M': result[cur] = '\x0D'; break;
                		case 'P': result[cur] = '\x10'; break;
                		case 'I': result[cur] = '\x09'; break;
                		case 'L': result[cur] = '\x0C'; break;
                		case 'E': result[cur] = '\x05'; break;
                		case 'R': result[cur] = '\x12'; break;
                		case 'S': result[cur] = '\x13'; break;
                		default:
                			printf("%s\n", "control character failed");
                			EM_error(charPos, yytext);
                			break;
                	}
                	break;
                }
                default:
                	// 3-Decimal ASCII character
                	if (str[i+1] >= '0' && str[i+1] <= '9') {
                		char *num = (char *)malloc(4);
                		strncpy(num, str+i+1, 3);
                		int ascii = atoi(num);
                		free(num);
                		result[cur] = (char)ascii;
                		i+=2; // need +4
                	}
                	else {
                		EM_error(charPos, yytext);
                		break;
                	}
            }
            cur++; 
            i += 2; // fit nomal escape character
		}
		else {
			result[cur] = str[i];
			cur++; i++;
		}
	}
    return result;
}

int nest_level = 0; // deal with nest comment

%}
  /* You can add lex definitions here. */
%Start COMMENT

%%
  /* 
  * Below is an example, which you can wipe out
  * and write reguler expressions and actions of your own.
  * //"\n" {adjust(); EM_newline(); continue;}
  */ 
	/* nest comment */
<INITIAL>"/*" {adjust(); nest_level++; BEGIN COMMENT;}
<COMMENT>"/*" {adjust(); nest_level++;}
<COMMENT>"*/" {adjust(); nest_level--; if (nest_level == 0) {BEGIN INITIAL;}}
<COMMENT>.|\n {adjust();}
	/* Reserved word */
<INITIAL>"while" {adjust(); return WHILE;}
<INITIAL>"for" {adjust(); return FOR;}
<INITIAL>"to" {adjust(); return TO;}
<INITIAL>"break" {adjust(); return BREAK;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"in" {adjust(); return IN;}
<INITIAL>"end" {adjust(); return END;}
<INITIAL>"function" {adjust(); return FUNCTION;}
<INITIAL>"var" {adjust(); return VAR;}
<INITIAL>"type" {adjust(); return TYPE;}
<INITIAL>"array" {adjust(); return ARRAY;}
<INITIAL>"if" {adjust(); return IF;}
<INITIAL>"then" {adjust(); return THEN;}
<INITIAL>"else" {adjust(); return ELSE;}
<INITIAL>"do" {adjust(); return DO;}
<INITIAL>"of" {adjust(); return OF;}
<INITIAL>"nil" {adjust(); return NIL;}
	/* Punctuation */
<INITIAL>"," {adjust(); return COMMA;}
<INITIAL>":" {adjust(); return COLON;}
<INITIAL>";" {adjust(); return SEMICOLON;}
<INITIAL>"(" {adjust(); return LPAREN;}
<INITIAL>")" {adjust(); return RPAREN;}
<INITIAL>"[" {adjust(); return LBRACK;}
<INITIAL>"]" {adjust(); return RBRACK;}
<INITIAL>"{" {adjust(); return LBRACE;}
<INITIAL>"}" {adjust(); return RBRACE;}
<INITIAL>"." {adjust(); return DOT;}
<INITIAL>"+" {adjust(); return PLUS;}
<INITIAL>"-" {adjust(); return MINUS;}
<INITIAL>"*" {adjust(); return TIMES;}
<INITIAL>"/" {adjust(); return DIVIDE;}
<INITIAL>"=" {adjust(); return EQ;}
<INITIAL>"<>" {adjust(); return NEQ;}
<INITIAL>"<" {adjust(); return LT;}
<INITIAL>"<=" {adjust(); return LE;}
<INITIAL>">" {adjust(); return GT;}
<INITIAL>">=" {adjust(); return GE;}
<INITIAL>"&" {adjust(); return AND;}
<INITIAL>"|" {adjust(); return OR;}
<INITIAL>":=" {adjust(); return ASSIGN;}
	/* unused */
<INITIAL>(" "|"\t")+ {adjust();}
<INITIAL>"\n" {adjust(); EM_newline();}
	/* ID INT STRING note " can be within a string */
<INITIAL>[a-zA-Z][a-zA-Z_0-9]* {adjust(); yylval.sval=String(yytext); return ID;}
<INITIAL>[0-9]+ {adjust(); yylval.ival=atoi(yytext); return INT;}
<INITIAL>\"([^\"]|\\\")*\" {adjust(); yylval.sval=getstr(yytext); return STRING;}
	
<INITIAL>. {adjust(); EM_error(charPos, yytext);}

