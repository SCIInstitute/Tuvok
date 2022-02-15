/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison GLR parsers in C

   Copyright (C) 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     INTEGER = 258,
     DOUBLE = 259,
     VOLUME = 260,
     OPEN_BRACKET = 261,
     CLOSE_BRACKET = 262,
     OPEN_PAREN = 263,
     CLOSE_PAREN = 264,
     QUESTION_MARK = 265,
     COLON = 266,
     BAD = 267,
     EQUAL_TO = 268,
     LESS_THAN = 269,
     GREATER_THAN = 270,
     MULTIPLY = 271,
     DIVIDE = 272,
     MINUS = 273,
     PLUS = 274
   };
#endif


/* Copy the first part of user declarations.  */
#line 28 "tvk-parse.ypp"

#define YYPARSE_PARAM scanner
#define YYLEX_PARAM   scanner
#include <cstdio>
#include <iostream>

#include "treenode.h"

#include "binary-expression.h"
#include "constant.h"
#include "volume.h"


#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE 
#line 52 "tvk-parse.ypp"
{
  double y_dbl;
  tuvok::expression::Node* y_tree;
  enum tuvok::expression::OpType y_otype;
}
/* Line 2616 of glr.c.  */
#line 87 "./tvk-parse.parser.hpp"
	YYSTYPE;
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{

  int first_line;
  int first_column;
  int last_line;
  int last_column;

} YYLTYPE;
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif


extern YYSTYPE yylval;




