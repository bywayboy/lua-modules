#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "automem.h"

void automem_init(automem_t* pmem, unsigned int initBufferSize)
{ 
	pmem->size = 0;
	pmem->buffersize = (initBufferSize == 0 ? 128 : initBufferSize);
	pmem->_malloc = malloc;
	pmem->_realloc = realloc;
	pmem->_free= free;
	pmem->pdata = (unsigned char*) pmem->_malloc(pmem->buffersize);

	
}

void automem_uninit(automem_t* pmem)
{
	if(NULL != pmem->pdata)
	{
		pmem->size = pmem->buffersize = 0;
		pmem->_free(pmem->pdata);
		pmem->pdata = NULL;
	}
}
/*清理并重新初始化*/
void automem_clean(automem_t* pmen, int newsize)
{
	automem_uninit(pmen);
	automem_init(pmen, newsize);
}

void automem_reset(automem_t* pmem)
{
	pmem->size = 0;
}

void automem_ensure_newspace(automem_t* pmem, unsigned int len)
{
	unsigned int newbuffersize = pmem->buffersize;
	while(newbuffersize - pmem->size < len)
	{
		newbuffersize *= 2;
	}
	if(newbuffersize > pmem->buffersize)
	{
		pmem->pdata = (unsigned char*) pmem->_realloc(pmem->pdata, newbuffersize);
		pmem->buffersize = newbuffersize;
	}
}
void automem_init_by_ptr(automem_t* pmem, void* pdata, unsigned int len)
{
	pmem->_malloc = malloc;
	pmem->_realloc = realloc;
	pmem->_free= free;
	pmem->pdata = pdata;
	pmem->size = len;
}
/* 绑定到一块内存空间 */
void automem_attach(automem_t* pmem, void* pdata, unsigned int len)
{
	if(len > pmem->buffersize)
		automem_ensure_newspace(pmem, len - pmem->buffersize);
	pmem->size = len;
	memcpy(pmem->pdata, pdata, len);
}

//after automem_detach(), pmem is not available utill next automem_init()
//returned value must be pmem->_free(), if not NULL

/* 获取数据指针和长度 */

void* automem_detach(automem_t* pmem, unsigned int* plen)
{
	void* pdata = (pmem->size == 0 ? NULL : pmem->pdata);
	if(plen) *plen = pmem->size;
	return pdata;
}


int automem_append_voidp(automem_t* pmem, const void* p, unsigned int len)
{
	automem_ensure_newspace(pmem, len);
	memcpy(pmem->pdata + pmem->size, p, len);
	pmem->size += len;
	return len;
}

int automem_erase(automem_t * pmem, unsigned int size)
{
	if(size < pmem->size)
	{
		memmove(pmem->pdata, pmem->pdata + size, pmem->size - size);
		pmem->size -= size;
	}else{
		pmem->size = 0;
	}
	return pmem->size;
}

int automem_erase_ex(automem_t* pmem, unsigned int size,unsigned int limit)
{

	if(size < pmem->size)
	{
		unsigned int newsize = pmem->size - size;

		if(pmem->buffersize > (newsize + limit))
		{
			char * buffer = (char*) pmem->_malloc(newsize + limit);
			memcpy(buffer, pmem->pdata + size, newsize);
			pmem->_free(pmem->pdata);
			pmem->pdata = (unsigned char*)buffer;
			pmem->size = newsize;
			pmem->buffersize = newsize + limit;
		}	
		else
		{
			memmove(pmem->pdata, pmem->pdata + size, newsize);
			pmem->size = newsize;
		}
	}
	else
	{
		pmem->size = 0;
		if(pmem->buffersize > limit)
		{
			automem_uninit(pmem);
			automem_init(pmem, limit);
		}
	}
	return pmem->size;
	
}

void automem_append_int(automem_t* pmem, int n)
{
	automem_append_voidp(pmem, &n, sizeof(int));
}
void automem_append_pchar(automem_t* pmem, char* n)
{
	automem_append_voidp(pmem, &n, sizeof(char*));
}

void automem_append_char(automem_t* pmem, char c)
{
	automem_append_voidp(pmem, &c, sizeof(char));
}
void automem_append_byte(automem_t* pmem, unsigned char c)
{
	automem_append_voidp(pmem, &c, sizeof(unsigned char));
}


const char * byte2hex(unsigned char b)
{
	static const char * hex_table[] = {
		"00","01","02","03","04","05","06","07","08","09","0A","0B","0C","0D","0E","0F",
		"10","11","12","13","14","15","16","17","18","19","1A","1B","1C","1D","1E","1F",
		"20","21","22","23","24","25","26","27","28","29","2A","2B","2C","2D","2E","2F",
		"30","31","32","33","34","35","36","37","38","39","3A","3B","3C","3D","3E","3F",
		"40","41","42","43","44","45","46","47","48","49","4A","4B","4C","4D","4E","4F",
		"50","51","52","53","54","55","56","57","58","59","5A","5B","5C","5D","5E","5F",
		"60","61","62","63","64","65","66","67","68","69","6A","6B","6C","6D","6E","6F",
		"70","71","72","73","74","75","76","77","78","79","7A","7B","7C","7D","7E","7F",
		"80","81","82","83","84","85","86","87","88","89","8A","8B","8C","8D","8E","8F",
		"90","91","92","93","94","95","96","97","98","99","9A","9B","9C","9D","9E","9F",
		"A0","A1","A2","A3","A4","A5","A6","A7","A8","A9","AA","AB","AC","AD","AE","AF",
		"B0","B1","B2","B3","B4","B5","B6","B7","B8","B9","BA","BB","BC","BD","BE","BF",
		"C0","C1","C2","C3","C4","C5","C6","C7","C8","C9","CA","CB","CC","CD","CE","CF",
		"D0","D1","D2","D3","D4","D5","D6","D7","D8","D9","DA","DB","DC","DD","DE","DF",
		"E0","E1","E2","E3","E4","E5","E6","E7","E8","E9","EA","EB","EC","ED","EE","EF",
		"F0","F1","F2","F3","F4","F5","F6","F7","F8","F9","FA","FB","FC","FD","FE","FF",
	};
	return hex_table[b];
}

unsigned char hex2byte(const unsigned char *ptr)
{
    int i;
    unsigned char result = 0;

    for (i = 0; i < 2; i++) {
        result <<= 4;
        result |= (ptr[i] >= '0' && ptr[i] <= '9') ? (ptr[i] - '0') :
            (((ptr[i] & 0xDF) >= 'A' && (ptr[i] & 0xDF) <= 'F') ? (ptr[i] - 'A' + 0x0A) :
            0x00);
    }

    return result;
}


