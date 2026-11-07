/* Genuine C consumer, no C++ ABI, engine, SQLite or provider import library.
 * Loads ONLY the explicit, test-only core DLL in the owned scratch directory.
 * Production callers must use GetModuleHandleW, not LoadLibrary on a mod.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bedrock_insight/core/stats_api.h"

typedef int (__cdecl *test_setup_fn)(const wchar_t*);
#define CHECK(condition) do { if(!(condition)) { \
    fprintf(stderr,"C API check failed, line %d: %s\n",__LINE__,#condition);goto finish; } } while(0)
#define RESOLVE(variable,name) do { FARPROC symbol=GetProcAddress(module,name); \
    _Static_assert(sizeof(variable)==sizeof(symbol),"Windows function pointer size"); \
    CHECK(symbol!=NULL);memcpy(&(variable),&symbol,sizeof(variable)); } while(0)

static unsigned char* read_fixture(const wchar_t* path,DWORD* length) {
    LARGE_INTEGER size;
    DWORD count=0;
    unsigned char* bytes=NULL;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)return NULL;
    if(GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<=1024*1024) {
        *length=(DWORD)size.QuadPart;
        bytes=(unsigned char*)malloc(*length);
        if(bytes && (!ReadFile(file,bytes,*length,&count,NULL) || count!=*length)) {
            free(bytes);bytes=NULL;
        }
    }
    CloseHandle(file);
    return bytes;
}

int wmain(int argc,wchar_t** argv) {
    int status=1;
    int written=0;
    char out[4096];
    char small[10];
    DWORD before_size=0,after_size=0;
    unsigned char* before=NULL;
    unsigned char* after=NULL;
    HMODULE module=NULL;
    test_setup_fn setup=NULL;
    bi_get_player_stats_fn player=NULL;
    bi_get_rank_stats_fn rank=NULL;
    bi_get_player_stats_page_fn player_page=NULL;
    bi_get_rank_stats_page_fn rank_page=NULL;
    CHECK(argc==3);
    module=LoadLibraryExW(argv[1],NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    CHECK(module!=NULL);
    RESOLVE(player,"getPlayerStats");RESOLVE(rank,"getRankStats");
    RESOLVE(player_page,"getPlayerStatsPage");RESOLVE(rank_page,"getRankStatsPage");
    CHECK(GetProcAddress(module,"set_remote_api_database")==NULL);
    CHECK(player("Alice","mined",out,sizeof(out))>0 && strstr(out,"\"ok\":false"));
    RESOLVE(setup,"bi_test_create_database");
    CHECK(setup(argv[2])==0);
    CHECK(setup(argv[2])<0); /* an existing file is never reset */
    before=read_fixture(argv[2],&before_size);CHECK(before!=NULL);
    written=player("Alice","minecraft:mined",out,sizeof(out));
    CHECK(written>0 && written==(int)strlen(out));
    CHECK(strstr(out,"\"total\":3") && strstr(out,"minecraft:stone") && !strstr(out,"private:id"));
    printf("JSON %s\n",out);
    CHECK(rank("mined",0,1,out,sizeof(out))>0);
    CHECK(strstr(out,"\"total_entries\":3") && strstr(out,"\"has_more\":true") && strstr(out,"Alice"));
    CHECK(rank_page("mined",1,1,out,sizeof(out))>0);
    CHECK(strstr(out,"\"rank\":2") && strstr(out,"Bob") && strstr(out,"\"has_more\":true"));
    CHECK(rank_page("mined",2,1,out,sizeof(out))>0);
    CHECK(strstr(out,"a\\\"b\\\\c\\n") && strstr(out,"\"has_more\":false") && !strstr(out,"private:id"));
    printf("JSON %s\n",out);
    CHECK(player_page("a\"b\\c\n","mined",0,1,out,sizeof(out))>0);
    CHECK(strstr(out,"a\\\"b\\\\c\\n") && strstr(out,"\"total\":1"));
    CHECK(rank_page("mined",BI_STATS_MAX_PAGE,BI_STATS_MAX_LIMIT,out,sizeof(out))>0);
    CHECK(strstr(out,"\"total_entries\":3") && strstr(out,"\"entries\":[]") && strstr(out,"\"has_more\":false"));
    printf("JSON %s\n",out);
    CHECK(rank_page("crafted",0,1,out,sizeof(out))>0);
    CHECK(strstr(out,"\"total_entries\":0") && strstr(out,"\"collection_complete\":false"));
    CHECK(player_page("NoData","crafted",0,1,out,sizeof(out))>0 && strstr(out,"\"total\":null"));
    CHECK(rank_page("custom:minecraft:damage_blocked_by_shield",0,1,out,sizeof(out))>0);
    CHECK(strstr(out,"\"ok\":false")); /* unknown metric cannot become zero */
    CHECK(rank("mined",BI_STATS_MAX_PAGE+1,1,out,sizeof(out))==-2 && out[0]=='\0');
    CHECK(rank("mined",0,BI_STATS_MAX_LIMIT+1,out,sizeof(out))==-2 && out[0]=='\0');
    CHECK(player(NULL,"mined",out,sizeof(out))==-1 && out[0]=='\0');
    CHECK(player("Alice","invalid",out,sizeof(out))==-4 && out[0]=='\0');
    CHECK(player("Alice","mined",NULL,0)==-2);
    memset(small,'X',sizeof(small));
    CHECK(player("Alice","mined",small,8)==-3 && small[0]=='\0' && small[1]=='X' && small[9]=='X');
    CHECK(player("Alice","mined",out,sizeof(out))>0 && strstr(out,"\"total\":3"));
    after=read_fixture(argv[2],&after_size);CHECK(after!=NULL);
    CHECK(before_size==after_size && memcmp(before,after,before_size)==0);
    puts("C dynamic API PASS: four GetProcAddress calls, JSON, escaped names, pages, no fabricated zero; core fixture only, NOT BDS/LRC acceptance");
    status=0;
finish:
    free(before);free(after);
    if(module)FreeLibrary(module);
    return status;
}
