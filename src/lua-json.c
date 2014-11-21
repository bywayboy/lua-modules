/*
License: MIT
Author: bywayboy<bywayboy@gmail.com>
Date: 2014-11-21
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lua-json.h"

#include "automem.h"

enum{
	json_error_ok = 0,
	json_error_bad_value,
	json_error_bad_separator,
	json_error_bad_character,
	json_error_bad_string,
	json_error_array_noclose,
	json_error_object_noclose,
};
enum{
	lt_string,
	lt_string_escape,
	lt_string_escape_unicode,

	lt_array_normal,
	lt_array_sep,

	lt_object_normal,
	lt_object_sep,	//	:
	lt_object_sep2, //	,
	lt_object_value,
};

#define		ch_is_space( x) (x == ' ') || (x == '\t') || (x == '\r') || (x == '\n')
#define		ch_is_hex( x )	\
(('0'<=(x) && '9'>=(x)) || ('a'<=(x) && 'f'>=(x)) || ('A'<=(x) && 'F'>=(x)))


typedef struct jsontok jsontok_t;
typedef struct jsonenc jsonenc_t;
struct jsontok{
	lua_State * L;
	char * jstr;
	int pos,size;
	int row;// line
	int col; // charator
	int err;
	automem_t mem;
};
struct jsonenc{
	lua_State * L;
	automem_t mem;
	int	unescape_utf;
};
#define JSONTOK_NEWLINE( tok ) (tok)->row++; (tok)->col=(tok)->pos

static int _lua_json_parse_value(jsontok_t * tok);

static int _lua_json_parser_key(jsontok_t *tok){
	int i=0;
	char c;
	while(tok->pos < tok->size)
	{
		c = tok->jstr[tok->pos+i];
		//如果是合法的,继续下一个
		if(('a'<=c && 'z'>=c) || ('A'<=c && 'Z'>=c) ) goto parser_key_continue; //-- 属性名称可以是字母
		if(i==0 && '_'==c) goto parser_key_continue;			//-- 属性名称可以是下划线开头.
		if(i >0 && ('0'<=c && '9'>=c))goto parser_key_continue; //-- 属性名称中间是可以允许出现数字的.

		// key 还是有数据的.
		if(i >0){
			lua_pushlstring(tok->L, &tok->jstr[tok->pos],i);
			tok->pos+=i;
			return 0;
		}
		break;
parser_key_continue:
		i++;tok->pos;
		continue;
	}
	tok->err = json_error_bad_character;
	tok->pos = i;
	return -1;
}
static int _lua_json_parse_string(jsontok_t * tok, const char quote)
{
	char c;
	int ps =tok->pos,s =lt_string;
	if(0 == quote)
		return _lua_json_parser_key(tok);
	while(tok->pos < tok->size){
		c = tok->jstr[tok->pos];
		switch(s){
		case lt_string:
			switch(c){
				case '\n': tok->pos++; JSONTOK_NEWLINE(tok); break;	// new line.
				case '\\': tok->pos++; s=lt_string_escape;break;	// switch to escape string.
				case '\'':case '"':
					if(c != quote){
						tok->pos++;automem_append_byte(&tok->mem,c);break;
					}
					//TODO: finish the string.
					lua_pushlstring(tok->L, (char *)tok->mem.pdata,tok->mem.size);
					tok->mem.size=0;
					tok->pos++;
					return 0;
				default:
					tok->pos++;
					automem_append_byte(&tok->mem, c);
					break;
			}
			break;
		case lt_string_escape:
			switch(c){
			case 'b': automem_append_byte(&tok->mem, '\b');tok->pos++;break;
			case 'n': automem_append_byte(&tok->mem, '\n');tok->pos++;break;
			case 'r': automem_append_byte(&tok->mem, '\r');tok->pos++;break;
			case 't': automem_append_byte(&tok->mem, '\t');tok->pos++;break;
			case 'f': automem_append_byte(&tok->mem, '\f');tok->pos++;break;
			case '\\': automem_append_byte(&tok->mem, '\\');tok->pos++;break;
			case '/': automem_append_byte(&tok->mem, '/');tok->pos++;break;
			case 'u':
				s=lt_string_escape_unicode;
				tok->pos++;
				break;
			default:
				automem_append_byte(&tok->mem,'\\');automem_append_byte(&tok->mem,c);break;
			}
			break;
		case lt_string_escape_unicode:
			{	//--处理 4个字节的UNICODE
				char * pt=&tok->jstr[tok->pos];
				if(ch_is_hex(pt[0]) && ch_is_hex(pt[1]) && ch_is_hex(pt[2]) && ch_is_hex(pt[3])){
					//是正确的 UNICODE.
					unsigned short uni = (hex2byte((unsigned char *)&pt[0]) << 8) | hex2byte((unsigned char *)&pt[2]);
					if(0x80 > uni){ //1 bytes
						automem_append_byte(&tok->mem, uni & 0x7F);
					}else if(0x800 > uni){ //2 bytes
						automem_append_byte(&tok->mem, (0xC0 | ((uni >> 6) & 0x3F)));
						automem_append_byte(&tok->mem, 0x80 | (0x3F & uni));
					}else if(0x10000 > uni){ //3 bytes
						automem_append_byte(&tok->mem, (0xE0 | ((uni >> 12))));
						automem_append_byte(&tok->mem, (0xC0 | ((uni >> 6) & 0x3F)));
						automem_append_byte(&tok->mem, 0x80 | (0x3F & uni));
					}
					else if(0x110000 > uni){ // 4 bytes.
						automem_append_byte(&tok->mem, (0xF0 | ((uni >> 18) & 0x07)));
						automem_append_byte(&tok->mem, (0xE0 | ((uni >> 12) & 0x3F)));
						automem_append_byte(&tok->mem, (0xC0 | ((uni >> 6) & 0x3F)));
						automem_append_byte(&tok->mem, 0x80 | (0x3F & uni));
					}
					tok->pos+=4;
					s=lt_string;
					break;
				}
			}
			tok->pos++;
			s=lt_string;
			automem_append_voidp(&tok->mem,"\\u",2);automem_append_byte(&tok->mem,c);
			break;
		}
	}
	tok->err=json_error_bad_string;
	return -1;
}



static int _lua_json_parse_array(jsontok_t * tok){
	char c,s=lt_array_normal;
	int i=0,r,idx=1;
	lua_checkstack(tok->L, 5);
	lua_newtable(tok->L);
	while(tok->pos < tok->size)
	{
		c= tok->jstr[tok->pos];
		switch(s){
		case lt_array_normal:
			switch(c){
			case '\n': tok->pos++; JSONTOK_NEWLINE(tok); break;	// new line.
			case '\r':case '\t':case ' ': tok->pos++; break;	// skip space character
			case ',':											//  double ,
				tok->err = json_error_bad_separator;
				return -1;
			case '[':
				tok->pos++;
				if(0 > (r = _lua_json_parse_array(tok)))
					return r;
				s=lt_array_sep;
				lua_rawseti(tok->L,-2,idx++);
				break;
			case ']':
				return 0;
			default:
				if(0 > (r=_lua_json_parse_value(tok))){
					lua_pop(tok->L,1);
					return r;
				}
				s=lt_array_sep;
				lua_rawseti(tok->L,-2,idx++);
				break;
			}
			break;
		case lt_array_sep:
			switch(c){
			case '\n': tok->pos++; JSONTOK_NEWLINE(tok);break;	// new line.
			case '\r':case '\t':case ' ': tok->pos++;break;		// skip space character
			case ',': tok->pos++; s=lt_array_normal; break;		//  has to next array elm.
			case ']': tok->pos++; return 0; // array parse filish.
			default: //!!! got error .
				tok->err = json_error_bad_character;
				return -1;
			}
			break;
		}

	}
	tok->err = json_error_array_noclose;
	return -1;
}

static int _lua_json_parser_object(jsontok_t * tok){
	char c,s = lt_object_normal;
	int r;
	lua_checkstack(tok->L, 5);
	lua_newtable(tok->L);
	while(tok->pos < tok->size){
		c=tok->jstr[tok->pos];
		switch(s){
		case lt_object_normal: //解析属性名称的部分.
			switch(c){
			case '\n': tok->pos++;JSONTOK_NEWLINE(tok);break;
			case '\r':case '\t':case ' ':tok->pos++;break;
			case '"':
			case '\'':
				tok->pos++;
				if(0 > (r= _lua_json_parse_string(tok, c)))
					return r;
				s=lt_object_sep;
				break;
			case '}':
				tok->pos++;
				return 0;
			default:
				if('_'==c || ('a'<=c && 'z'>=c) || ('A'<=c && 'Z'>=c))
				{	if(0 > (r= _lua_json_parse_string(tok, '\0')))
						return r;
					s=lt_object_sep;
					break;
				}
				tok->err = json_error_bad_character;
				return -1;
			}
			break;
		case lt_object_sep:
			switch(c){
			case '\n': tok->pos++;JSONTOK_NEWLINE(tok);break;
			case '\r':case '\t':case ' ':tok->pos++;break;
			case ':':
				tok->pos++;
				s=lt_object_value;
				break;
			default:
				tok->err = json_error_bad_character;
				return -1;
			}
			break;
		case lt_object_value:
			if(0 > (r= _lua_json_parse_value(tok)))
				return r;
			lua_settable(tok->L, -3);
			s=lt_object_sep2;
			break;
		case lt_object_sep2:
			switch(c){
			case '\n': tok->pos++; JSONTOK_NEWLINE(tok);break;	// new line.
			case '\r':case '\t':case ' ': tok->pos++;break;		// skip space character
			case ',': tok->pos++; s=lt_object_normal; break;		//  has to next array elm.
			case '}': tok->pos++; return 0; // array parse filish.
			default: //!!! got error .
				tok->err = json_error_bad_character;
				return -1;
			}
			break;
		}
	}
	tok->err =json_error_object_noclose;
	return -1;
}
static int _lua_json_parser_number(jsontok_t * tok){
	char c;
	int i=tok->pos ,isnum;
	lua_Number num;
	do{
		c = tok->jstr[i++];
		if(('+'==c || '-' == c) || (c >='0' && c <='9') || (c >='a' && c <='f') || (c >='A' && c <='F') || ('.'==c || 'e'==c || 'E'==c || 'x'==c || 'X'==c)){
			continue;
		}
		break;
	}while(1);
	lua_pushlstring(tok->L, &tok->jstr[tok->pos], i-tok->pos-1);
	num = lua_tonumberx(tok->L, -1, &isnum);
	if(num == 0){
		tok->err = json_error_bad_value;
		return -1;
	};
	lua_pushnumber(tok->L, num);
	lua_replace(tok->L, -2);
	tok->pos +=(i-tok->pos-1);
	return 0;
}
// 解析器入口.
static int _lua_json_parse_value(jsontok_t * tok)
{
	size_t i =0,r;
	char c;
	while(tok->pos < tok->size)
	{
		c = tok->jstr[tok->pos];
		switch(c){
		case '\'':
		case '"':
			tok->pos++;
			return _lua_json_parse_string(tok, c);
			//puts(tok->jstr[tok->pos]);
			break;
		case '\n':
			tok->pos++;JSONTOK_NEWLINE(tok);
			break;
		case '\r':case '\t':case ' ': // space character.
			tok->pos++;
			break;
		case '[':
			tok->pos++;
			return r =  _lua_json_parse_array(tok);
			break;
		case '{':
			tok->pos++;
			return _lua_json_parser_object(tok);
		case 't':case 'T':
			lua_pushboolean(tok->L,1);
			tok->pos +=4;
			return 0;
			//maby is true.
			break;
		case 'f':case 'F':
			lua_pushboolean(tok->L,0);
			tok->pos +=5;
			return 0;
			// maby is false.
			break;
		case 'n':case 'N':
			//maby is null.
			lua_pushnil(tok->L);
			tok->pos +=4;
			return 0;
		case '+':case '-':
		case '0':case '1':case '2':case '3':case '4':case '5':case '6':case '7':case '8':case '9':
			return _lua_json_parser_number(tok);
		default:
			tok->pos++;
			break;
		}
	}
	tok->err=json_error_bad_character;
	return -1;
}
static const char * json_errstr(int v){
	char * errstr = "unknown error";
	switch(v)
	{
	case json_error_bad_value:
		errstr="bad value";
		break;
	case json_error_bad_separator:
		errstr="bad separator";
		break;
	case json_error_bad_character:
		errstr="bad character";
		break;
	case json_error_bad_string:
		errstr="bad string";
		break;
	case json_error_array_noclose:
		errstr="array no close ']'";
		break;
	case json_error_object_noclose:
		errstr="object no close '}'";
		break;
	}
	return errstr;
}
int lua_json_decode(lua_State * L)
{
	size_t lstr;
	int r,top;
	const char * str = lua_tolstring(L, 1, &lstr);
	if(NULL != str && lstr > 0){
		jsontok_t tok;
		tok.pos = tok.row=tok.col =0;
		tok.jstr = (char *)str;
		tok.size = lstr;
		tok.err = 0;
		tok.L = L;
		top = lua_gettop(L);
		automem_init(&tok.mem,128);
		if(0 >(r = _lua_json_parse_value(&tok))){
			lua_settop(L, top);
			lua_pushboolean(L, 0);
			lua_pushfstring(L,"json error: %s at line:%d col:%d", json_errstr(tok.err), tok.row,tok.pos-tok.col);
			goto json_parser_final;
		}
		lua_pushboolean(L,1);
		lua_insert(L,-2);
json_parser_final:
		automem_uninit(&tok.mem);
		return 2;
	}
	
	return 2;
}

static int json_encode_string(jsonenc_t * enc, char * vstr, int vlen){
	int i=0;
	unsigned char c,*utf8;
	unsigned short ucs2;
	automem_append_byte(&enc->mem,'"');
	while(i < vlen){
		c=vstr[i];
		switch(c){
		case '"':
			i++;automem_append_voidp(&enc->mem,"\\\"",2);break;
		case '\'':
			i++;automem_append_voidp(&enc->mem,"\\\'",2);break;
		case '\n':
			i++;automem_append_voidp(&enc->mem,"\\\n",2);break;
		case '\t':
			i++;automem_append_voidp(&enc->mem,"\\\t",2);break;
		case '\r':
			i++;automem_append_voidp(&enc->mem,"\\\r",2);break;
		case '\b':
			i++;automem_append_voidp(&enc->mem,"\\\b",2);break;
		case '\f':
			i++;automem_append_voidp(&enc->mem,"\\\f",2);break;
		default:
			if(enc->unescape_utf){
				i++;automem_append_byte(&enc->mem,c);
				break;
			}
			if(c < 0x80){
				i++;automem_append_byte(&enc->mem,c);
			}else if( 0xC0== (c & 0xE0)){ // 2 bytes.
				utf8 = (unsigned char *)&vstr[i];
				ucs2 = (*utf8 & 0x3F)<<6 | (*(utf8+1) & 0x3F);
				automem_append_voidp(&enc->mem,"\\u",2);
				automem_append_voidp(&enc->mem, byte2hex((ucs2 >> 8) & 0xFF),2);
				automem_append_voidp(&enc->mem, byte2hex(ucs2 & 0xFF),2);
				i+=2;
			}else if(0xE0 == (c & 0xF0)){// 3 bytes.
				utf8 = (unsigned char *)&vstr[i];
				ucs2 = (*utf8 & 0x1F)<<12 | (*(utf8+1) & 0x3F)<<6 | (*(utf8+2) & 0x3F);
				automem_append_voidp(&enc->mem,"\\u",2);
				automem_append_voidp(&enc->mem, byte2hex((ucs2 >> 8) & 0xFF),2);
				automem_append_voidp(&enc->mem, byte2hex(ucs2 & 0xFF),2);
				i+=3;
			}else if(0xF0 == (c & 0xF8)){// 4 bytes. !!!ucs2 out of range 
				i++;automem_append_byte(&enc->mem,c);				
			}
			break;
		}
	}
	automem_append_byte(&enc->mem,'"');
	return enc->mem.size;
}

static int json_encode_value(jsonenc_t * enc){
	int i=0,t=lua_type(enc->L, -1);
	size_t lkey;
	char s_close=']', * key;
	lua_checkstack(enc->L, 5);
	switch(t){
	case LUA_TTABLE:
		lua_pushnil(enc->L);
		while(lua_next(enc->L, -2)){
			if(i == 0){
				if(LUA_TSTRING == lua_type(enc->L, -2)){
					automem_append_byte(&enc->mem, '{');
					s_close = '}';
					key = (char *)luaL_tolstring(enc->L, -2, &lkey);
					json_encode_string(enc, key,lkey);
					lua_pop(enc->L, 1);  //-- lua checklstring change stack
					automem_append_byte(&enc->mem, ':');
					json_encode_value(enc);
				}else{
					automem_append_byte(&enc->mem, '[');
					json_encode_value(enc);
				}
				i++;
			}else{
				automem_append_byte(&enc->mem, ',');
				if(s_close == '}'){
					key = (char *)luaL_tolstring(enc->L, -2, &lkey);
					json_encode_string(enc, key,lkey);
					lua_pop(enc->L, 1);  //-- lua checklstring change stack
					automem_append_byte(&enc->mem, ':');
					json_encode_value(enc);
				}else{
					json_encode_value(enc);
				}
			}
			lua_pop(enc->L,1);// -- popup the value
		}
		if(0==i)
			automem_append_byte(&enc->mem, '[');
		automem_append_byte(&enc->mem, s_close);
		break;
	case LUA_TSTRING:
		key = (char *)luaL_tolstring(enc->L, -1, &lkey);
		json_encode_string(enc, key,lkey);
		lua_pop(enc->L,1);	 //-- lua checklstring change stack
		break;
	case LUA_TNUMBER:
		key = (char *)luaL_tolstring(enc->L, -1, &lkey);
		automem_append_voidp(&enc->mem, key, lkey);
		lua_pop(enc->L,1);	 //-- lua checklstring change stack
		break;
	case LUA_TBOOLEAN:
		if(lua_toboolean(enc->L,-1))
			automem_append_voidp(&enc->mem, "true",4);
		else
			automem_append_voidp(&enc->mem, "false",5);
		break;
	default:
		automem_append_voidp(&enc->mem, "null",4);
		break;
	}
	return 0;
}

int lua_json_encode(lua_State * L)
{
	jsonenc_t enc;
	char * str;
	size_t lstr;
	int v,t=lua_type(L, 1);
	enc.L=L;
	enc.unescape_utf = 0;
	printf("arguments = %d\n",lua_gettop(L));
	if(lua_gettop(L) > 1)
		enc.unescape_utf = lua_toboolean(L, 2);
	switch(t){
	case LUA_TTABLE:
		enc.L = L;
		automem_init(&enc.mem,128);
		lua_pushvalue(L,1); // -- argument 1 to stack top.
		json_encode_value(&enc);
		lua_pushlstring(L,(char *)enc.mem.pdata, enc.mem.size);
		automem_uninit(&enc.mem);
		break;
	case LUA_TSTRING:
		automem_init(&enc.mem,128);
		str = (char *)lua_tolstring(L,1,&lstr);
		json_encode_string(&enc,str,lstr);
		lua_pushlstring(L,(char *)enc.mem.pdata, enc.mem.size);
		automem_uninit(&enc.mem);
		break;
	case LUA_TNUMBER:
		str = (char *)luaL_tolstring(L, 1, &lstr);
		lua_pushlstring(L, str, lstr);
		break;
	case LUA_TBOOLEAN:
		v = lua_toboolean(L,1);
		lua_pushstring(L, v?"true":"false");
		break;
	case LUA_TNIL:
	case LUA_TNONE:
		lua_pushstring(L,"null");
		break;
	default:
		lua_pushstring(L,"null"); //-- 其它未知类型
		break;
	}
	return 1;
}
