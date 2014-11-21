#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <io.h>
#else
	#define O_BINARY	0
	#include <unistd.h>
#endif

#include "automem.h"

#include "lua.h"
#include "lauxlib.h"

#include "lua-tpl.h"

/*
	LUA 最简单的模板引擎.
*/

const char LUAFMT_TPL_META[]="LUAFMT_TPL";

void lua_cfmt_createmetatable (lua_State *L);
void lua_register_class(lua_State * L,const luaL_Reg * methods,const char * name,lua_CFunction has_index);

#if defined(_WIN32) || defined(_WIN64)
	#define open(x, y)	_open(x, y)
	#define close(x)	_close(x)
	#define read(x, y, z)	_read(x, y, z)
	#define lseek( x, y, z)		_lseek(x, y, z)
	#define stat( x , y )	_stat( x , y )
#endif

static char * file_get_contents(const char * file,int * sz){
	char * ret = NULL;
	struct stat st;
	int fd;
	*sz = 0;
	if ((fd = open(file, O_RDONLY|O_BINARY)) != -1)
	{
		if (fstat(fd, &st) != -1)
		{
			ret = (char *)malloc(st.st_size);
			if(NULL != ret){
				while( *sz < st.st_size){
					*sz += read(fd, ret + *sz, st.st_size - (*sz));
					lseek(fd, *sz, SEEK_SET);
				}
			}
			
		}
		close(fd);
	}
	return ret;
}

static void lua_tpl_expand_variables(lua_State * L, automem_t * mem, int bufsize){
	if(lua_istable(L, 3)){
		const char * key ;
		automem_init(mem, bufsize + 10240);
		lua_pushnil(L);
		automem_append_voidp(mem, "local _ARGS_= ...\n",sizeof("local _ARGS_= ...\n")-1);
		while(lua_next(L, 3)){
			// -1 value -2 key
			if(lua_isstring(L, -2)){
				size_t lkey;
				if(NULL != (key = lua_tolstring(L, -2, &lkey))){
					automem_append_voidp(mem, "local ",sizeof("local ")-1);
					automem_append_voidp(mem, key, lkey);
					automem_append_voidp(mem, "=_ARGS_[\"",sizeof("=_ARGS_[\"")-1);
					automem_append_voidp(mem, key, lkey);
					automem_append_voidp(mem, "\"]\n", sizeof("\"]\n")-1);
				}
			}
			lua_pop(L, 1);
		}
	}else
		automem_init(mem, bufsize + 20);
}

/*成功返回文件的真实长度,失败返回 0*/
static int read_from_cached(lua_State * L,automem_t * mem,const char * file){
	struct stat st;
	int fd,sz = 0;

	if ((fd = open(file, O_RDONLY|O_BINARY)) != -1)
	{
		unsigned char * pdata ;
		if (fstat(fd, &st) != -1)
		{
			lua_tpl_expand_variables(L, mem, st.st_size );
			pdata = mem->pdata + mem->size;
			while( sz < st.st_size){
				sz += read(fd, pdata + sz, st.st_size - sz);
				lseek(fd, sz, SEEK_SET);
			}
			mem->size += sz;
		}
		close(fd);
	}
	return sz;
}

static int file_put_contents(const char* file, unsigned char * pdata, unsigned int size)
{
	FILE * fp = fopen(file,"wb+");

	if(NULL != fp){
		fwrite(pdata, size, 1, fp);
		fclose(fp);
		return 1;
	}
	return 0;
}

enum{
	tpl_state_normal,
	tpl_state_scode_1,
	tpl_state_code,
	tpl_state_ecode_1,
	tpl_state_escape,
};

#define append_end_stringfield(a,b,c) \
	automem_append_voidp( (a) , cmd, lcmd); \
	automem_append_voidp( (a) , prefix, lprefix); \
	automem_append_voidp( (a), (b) , (c)); \
	automem_append_voidp( (a) , suffix, lsuffix); \
	automem_append_byte( (a), '\n')

static void lua_tpl_compile_local(lua_State * L, automem_t * mem, const char * buf,int lbuf){
	int state = tpl_state_normal, i = 0;
	register const char * sbuf; char c;
	size_t lcmd = sizeof("_") -1,
		lprefix = sizeof("([[")-1,
		lsuffix = sizeof("]])")-1;

	const char * cmd = "_(",
		* ecmd = ")",
		* prefix = "([[",
		* suffix = "]])";

	if(lua_isstring(L, 4))
		cmd =luaL_checklstring(L, 4, &lcmd);

	if(lua_isstring(L, 5))
		prefix =luaL_checklstring(L, 5, &lprefix);

	if(lua_isstring(L, 6))
		suffix =luaL_checklstring(L, 6, &lsuffix);


	sbuf = buf;
	while(i < lbuf){
		c = buf[i];
		switch (state)
		{
		case tpl_state_normal:
			switch(c){
			case '{':
				state = tpl_state_scode_1;
				break;
			}
			break;
		case tpl_state_scode_1:
			switch(c){
			case '#':
				state =tpl_state_code;
				append_end_stringfield(mem,sbuf, &buf[i] - sbuf-1);
				sbuf = &buf[i+1];
				break;
			default:
				state = tpl_state_normal;
				break;
			}
			break;
		case tpl_state_code:
			switch(c){
			case '#':
				state = tpl_state_ecode_1;
				break;
			}
		case tpl_state_ecode_1:
			switch(c){
			case '}':
				automem_append_voidp(mem,sbuf, &buf[i] - sbuf-1);
				automem_append_byte(mem,'\n');
				sbuf = &buf[i+1];
				state = tpl_state_normal;
				break;
			default:
				state=tpl_state_code;
			}
		default:
			break;
		}
		i++;
	}
	if(tpl_state_normal == state){
		append_end_stringfield(mem,sbuf, &buf[i] - sbuf);
	}
}
/* 对模板文件进行编译.*/
int _lua_tpl_compile(lua_State * L)
{
	int cache = 0,lbuf = 0;
	unsigned int _var_size = 0;
	size_t lfile;

	const char *cfile = NULL, 
		* file= luaL_checklstring(L, 1,&lfile),
		* buf = NULL;
#if defined(_WIN32) || defined(_WIN64)
	automem_t mem={NULL};
#else
	automem_t mem={
		.size = 0,
		.pdata = NULL
	};
#endif

	if(lua_isboolean(L, 2))
		cache =lua_toboolean(L, 2);

	if(0 != cache){
		struct stat st1,st2;
		cfile=(char *)malloc(lfile+5);
		memcpy((char *)cfile,file,lfile);
		strcpy((char *)cfile+lfile,".tpl");
		if((0 == stat(file,&st1)) && (0 == stat(cfile, &st2)))
		{
			if(st1.st_mtime <= st2.st_mtime){
				read_from_cached(L, &mem, cfile);
				free((void*)cfile);
				lua_pushlstring(L, mem.pdata, mem.size);
				goto lua_tpl_compile_final;
			}
		}
	}

	if(NULL != (buf = file_get_contents(file, &lbuf)))
	{
		lua_tpl_expand_variables(L, &mem, lbuf);
		_var_size = mem.size;
		lua_tpl_compile_local(L, &mem, buf, lbuf);
		free((void*)buf);
		lua_pushlstring(L,(char *)mem.pdata,mem.size);
	}
	if(0 != cache && mem.pdata != NULL)
		file_put_contents(cfile, mem.pdata + _var_size, mem.size - _var_size);

	if(NULL != cfile)
		free((void *)cfile);
lua_tpl_compile_final:
	automem_uninit(&mem);
	return 1;
}


