// wCron  :  a unix style cron service for windows
//
#include "wcron.h"

struct stat laststat;
time_t      now;            			// current time
struct tm   *st;            			// start time
int         LOGmode=3;      			// log start/stop and load of crontab, and start stop errors
char        LOGinfodir[256]=""; 		// use info files for output
char        shutdowncmd[256]="";		// execute shutdowncmd in preshutdown phase
char        startupcmd[256]=""; 		// execute startupcmd in boot phase
int         maxcron=0;
int         maxrange=0;
int         noexec=0;
int         noerr=0;
int         noact=0;        			// no procs active
dynamic_array entry;				 	// dynamic array of entries
dynamic_array range; 					// dynamic array of ranges
dynamic_array cmdext; 					// dynamic array of command extensions
dynamic_array crons; 					// dynamic array of cron files

// Type-specific cleanup functions
void free_cmdext(void *item)
{
    t_cmdext *ext = (t_cmdext *)item;
    if (ext->ext)  free(ext->ext);
    if (ext->fn)   free(ext->fn);
    if (ext->opts) free(ext->opts);
}

void free_cronfile(void *item)
{
    t_cronfile *cron = (t_cronfile *)item;
    if (cron->fn) free(cron->fn);
}

void free_cronentry(void *item)
{
    cronentry *e = *(cronentry **)item;
    if (e->cmd)    free(e->cmd);
    if (e->param)  free(e->param);
    if (e->cmdstr) free(e->cmdstr);
    free(e);
}

void free_range(void *item)
{
    ranges *r = *(ranges **)item;
    free(r);
}

DWORD calchash(unsigned char *str)
{
    unsigned long hash = 5381;
    int c;
    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return (DWORD) hash;
}

char *make_cmdstr(char *cmd, char *params) 
{
    char tmp[256], *ptr;
	int  i;
    sprintf(tmp,"\"%s\" %s",cmd,params);
    ptr=strrchr(cmd,'.');
    if (ptr!=NULL && strcmp(ptr,".exe")!=0) {
        for (i=0; i<cmdext.size; i++) {
            t_cmdext *ext = (t_cmdext *)array_get(&cmdext, i);
            if (strcmp(ext->ext, &ptr[1])==0) {
                sprintf(tmp,"\"%s\" %s \"%s\" %s",ext->fn,ext->opts,cmd,params);
                break;
            }
        }
    }
    ptr=(char *)malloc(strlen(tmp)+1);
    if (ptr) strcpy(ptr,tmp);
    return ptr;
}

void add_cmdext(char *ext, char *fn, char *opts)
{
    t_cmdext *new_ext = (t_cmdext *)array_add(&cmdext);
    if (!new_ext) return;
    
    new_ext->ext  = (char *)malloc(strlen(ext)+1);
    new_ext->fn   = (char *)malloc(strlen(fn)+1);
    new_ext->opts = (char *)malloc(opts ? strlen(opts)+1 : 1);
    
    if (new_ext->ext && new_ext->fn && new_ext->opts) {
        strcpy(new_ext->ext, ext);
        strcpy(new_ext->fn, fn);
        strcpy(new_ext->opts, opts ? opts : "");
    } else {
        if (new_ext->ext)  free(new_ext->ext);
        if (new_ext->fn)   free(new_ext->fn);
        if (new_ext->opts) free(new_ext->opts);
        cmdext.size--; // Undo add
    }
}

void clr_cmdext(void)
{
    array_clear(&cmdext, free_cmdext);
}

void add_cronfile(char *fn)
{
    t_cronfile *new_cron = (t_cronfile *)array_add(&crons);
	if (!new_cron) {
		return;
	}
    new_cron->fn = (char *)malloc(strlen(fn)+1);
    if (new_cron->fn) {
        strcpy(new_cron->fn, fn);
        memset(&new_cron->stat, 0, sizeof(struct stat));
    } else {
        crons.size--; // Undo add
    }
}

void del_cronfile(char *fn)
{
	int i,j;
    for (i=0; i<crons.size; i++) {
        t_cronfile *cron = (t_cronfile *)array_get(&crons, i);
        if (strcmp(fn, cron->fn)==0) {
            free_cronfile(cron);
            for (j=i; j<crons.size-1; j++) {
                t_cronfile *next    = (t_cronfile *)array_get(&crons, j+1);
                t_cronfile *current = (t_cronfile *)array_get(&crons, j);
                *current = *next;
            }
            crons.size--;
            break;
        }
    }
}

void init_cronfile(char *fn)
{
    array_clear(&crons, free_cronfile);
    add_cronfile(fn);
}

// ============================ file parsing =================================
void add_range(int l, int h, int s)
{    
	ranges *new_range, **new_slot;

    new_slot = (ranges **)array_add(&range);
    if (!new_slot) return;
    
    new_range = (ranges *)malloc(sizeof(ranges));
    if (new_range) {
        new_range->low = l;
        new_range->high = h;
        new_range->step = s;
        *new_slot = new_range;
        maxrange++;
    } else {
        range.size--; // Undo add
    }
}

int getval(char *str, int dval, int mval)
{
    if (str[0]=='*')
        return mval;
    else if (str[0]=='?')
        return dval;
    else
        return atoi(str);
}

void parse_when(char *str, int dval, int mval, int *fr, int *un)
{
    char *s1, *s2, *p1, *p2;
    int l, h, s;
    if (strcmp(str,"*")==0) {
        *fr = *un = 0;
        return;
    }
    *fr = maxrange;
    s1 = str;
    do {
        s2 = strchr(s1,',');
        if (s2) *s2 = 0;
        
        if ((p1=strchr(s1,'/'))!=0) { // step range  */15 or 40/5
            *p1 = 0;
            if ((p2=strchr(s1,'-'))!=0) { // ex.  21-51/5
                *p2 = 0;
                l = getval(s1,dval,mval);
                h = getval(&p2[1],dval,mval);
            } else {
                l = 0;
                h = getval(s1,dval,mval);
            }
            s = getval(&p1[1],dval,mval);
            add_range(l,h,s);
        } else if ((p1=strchr(s1,'-'))!=0) { // range
            *p1 = 0;
            l = getval(s1,dval,mval);
            h = getval(&p1[1],dval,mval);
            add_range(l,h,1);
        } else { // single value
            l = getval(s1,dval,mval);
            add_range(l,l,1);
        }
        s1 = s2;
        if (s1) s1++;
    } while (s1);
    *un = maxrange-1;
}

int next_field(char *src, char *dst, int maxlen)
{
    char *ptr, *tmp;
    int len, pre;

    pre = 0;
    for (ptr=src; (*ptr==' ' || *ptr=='\t'); ptr++) pre++;
    len = 0;
    for (tmp=ptr; (*ptr!=' ' && *ptr!='\t'); ptr++) len++;
    ptr = tmp;

    if (len>maxlen) {
        if (dst) dst[0]=0;
        return 0;
    }
    if (dst) {
        strncpy(dst,ptr,len);
        dst[len]='\0';
    }
    return pre+len;
}

int count_ranges(char *str, int *valid)
{
    int i, pos, no, nostars;
    char fld[5][81], *ptr;

    *valid = 0;
    for (pos=i=0; i<5; i++) {
        fld[i][0]='\0';
        no = next_field(&str[pos],fld[i],80);
        pos += no+1;
    }
    if (no==0) return 0; // skip entry

    for (nostars=no=i=0; i<5; i++) {
        if (fld[i][0]!='\0') {
            if (strcmp(fld[i],"*")!=0) {
                no++;
                for (ptr=fld[i]; *ptr!=0; ptr++) if (*ptr==',') no++;
            } else
                nostars++;
        }
    }
    if (no || nostars==5) *valid=1;
    return no;
}

void add_entry(char *str)
{
    int i, pos, len, log;
    char *ptr, *cmd, *param, *tmp;
    char fld[5][81];
    char buf[256];
    cronentry **new_slot, *new_entry;

    log = LOGmode;
    for (pos=i=0; i<5; i++) {
        len = next_field(&str[pos],fld[i],80);
        pos += len+1;
    }
    if (len==0) return;
    for (ptr=&str[pos]; (*ptr==' ' || *ptr=='\t'); ptr++);

    switch (*ptr) {
        case '~' : log=0; ptr++; break;
        case '-' : log=2; ptr++; break;
        case '=' : log=3; ptr++; break;
        case '+' : log=9; ptr++; break;
    }

    if (*ptr=='"') {
        ptr++;
		cmd=ptr;
        param = strchr(ptr,'"');
        if (param==0) return;
        *param++ = 0;
    } else {
		cmd=ptr;
        param = strchr(ptr,' ');
        if (param==0) {
            for (param=ptr; *param!=0; param++);
        } else
            *param++ = 0;
    }
    while (*param!=0 && *param==' ') param++;
	// only if file exists otherwise return;
    if (Texists(cmd,0) == 0) return;

    tmp = strstr(param," >");
    if (tmp) *tmp = 0;
    tmp = strstr(param," 1>");
    if (tmp) *tmp = 0;
    tmp = strstr(param," 2>");
    if (tmp) *tmp = 0;

	new_slot = (cronentry **)array_add(&entry);
    if (!new_slot) return;

    new_entry = (cronentry *)malloc(sizeof(cronentry));
    if (!new_entry) {
        entry.size--;
        return;
    }
    LOGprintf("found cmd=%s  param=%s\n",cmd,param);
    new_entry->log = log;
    new_entry->cmd = (char *)malloc(strlen(ptr)+1);
    if (new_entry->cmd) strcpy(new_entry->cmd, ptr);
    while ((ptr=strchr(cmd,'/'))!=NULL) *ptr='\\';

    new_entry->param = (char *)malloc(strlen(param)+1);
    if (new_entry->param) strcpy(new_entry->param, param);
    new_entry->hproc = 0;
    new_entry->cmdstr = make_cmdstr(cmd, param);

    if (LOGmode>=0) {
        parse_when(fld[0], st->tm_min, 59, &new_entry->m1, &new_entry->m2);
        parse_when(fld[1], st->tm_hour, 23, &new_entry->h1, &new_entry->h2);
        parse_when(fld[2], st->tm_mday, 31, &new_entry->d1, &new_entry->d2);
        parse_when(fld[3], st->tm_mon+1, 12, &new_entry->M1, &new_entry->M2);
        parse_when(fld[4], st->tm_wday, 6, &new_entry->w1, &new_entry->w2);
    }
    sprintf(buf,"%s_%s",pBaseName,str);
    new_entry->hash = calchash((unsigned char *)buf);

    sprintf(buf,"%*.*s",pos,pos,str);
    for (ptr=&str[pos]; (*ptr==' ' || *ptr=='\t'); ptr++);
    if (LOGmode<0) {
        printf("- entry %-24s %08x %s %s\n",buf,new_entry->hash,ptr,new_entry->param);
    } else {
        LOGprintf("- entry %-24s %08x %s %s\n",buf,new_entry->hash,ptr,new_entry->param);
    }

    *new_slot = new_entry;
    maxcron++;
}

void clear_entries(void)
{
    array_clear(&entry, free_cronentry);
    array_clear(&range, free_range);
}

void stripcrlf(char *str)
{
    int i;
    for (i=(int)strlen(str)-1; (i>=0 && str[i]<=32); i--) str[i]=0;
}

int count_cronentries(int *entries, int *ranges)
{
    FILE *fp;
    int i, n, no_e, no_r, valid;
    char buf[513];

    no_r = 1;
    no_e = 0;
    for (i=0; i<crons.size; i++) {
        t_cronfile *cron = (t_cronfile *)array_get(&crons, i);
        fp = fopen(cron->fn,"r");
        if (fp==NULL) {
            LOGprintf("cron.tab file not found [%s]\n",cron->fn);
            del_cronfile(cron->fn);
            i--;
        } else {
            LOGprintf("Loading crontab file %s ....\n",cron->fn);
            while (fgets(buf,512,fp)!=0) {
                stripcrlf(buf);
                if (buf[0]=='$' || buf[0]=='@') {
                    if (strnicmp(&buf[1],"cronfile=",9)==0) add_cronfile(&buf[10]);
                } else if (buf[0]!='#' && strlen(buf)>8) {
                    if ((n=count_ranges(buf,&valid))!=0) {
                        no_r += n;
                    }
                    if (valid) 
                        no_e++;
                    else
                        LOGprintf("Invalid entry : %s\n",buf);
                }
            }
            fclose(fp);
            stat(cron->fn, &cron->stat);
        }
    }
    *entries = no_e;
    *ranges = no_r;
    return no_e;
}

void parse_cronline(char *buf)
{
    int valid;
    stripcrlf(buf);
    if (buf[0]=='$' || buf[0]=='@') {
        if (LOGmode>=0) {
            LOGprintf("# cron cmd: %s\n",buf);
            if (strnicmp(&buf[1],"log=",4)==0) LOGmode=atoi(&buf[5]);
            if (strnicmp(&buf[1],"loginfodir=",11)==0) strcpy(LOGinfodir,&buf[12]);
            if (strnicmp(&buf[1],"shutdowncmd=",12)==0) strcpy(shutdowncmd,&buf[13]);
            if (strnicmp(&buf[1],"startupcmd=",11)==0) strcpy(startupcmd,&buf[12]);
            if (strnicmp(&buf[1],"cmdext=",7)==0) {
                char *pfn, *popt;
                pfn = strchr(&buf[8],';');
                if (pfn) {
                    *pfn++ = '\0';
                    popt = strchr(pfn,';');
                    if (popt) *popt++ = '\0';
                    if (Texists(pfn,0)) add_cmdext(&buf[8],pfn,popt?popt:"");
                }
            }
        }
    } else if (buf[0]!='#' && strlen(buf)>8) {
        count_ranges(buf,&valid);
        if (valid) add_entry(buf);
    }
}

void read_cronentries(void)
{
    FILE *fp;
    ranges **default_slot;
    int i, no_e, no_r;
    char buf[513];

    init_cronfile(pTabName);    
    clear_entries();
    if (noexec || noerr) {
        LOGprintf("stats executes %d   errors %d\n",noexec,noerr);
        noexec = noerr = 0;
    }
    if (count_cronentries(&no_e,&no_r)==0) {
        LOGprintf("No valid cronjob entries in cron.tab\n");
        return;
    }
    LOGprintf("Found %d cronjobs and %d time ranges in %d cron files\n",no_e,no_r,crons.size);

    // Initialize default range
    default_slot = (ranges **)array_add(&range);
    if (default_slot) {
        ranges *default_range = (ranges *)malloc(sizeof(ranges));
        if (default_range) {
            default_range->low = 0;
            default_range->high = 99;
            default_range->step = 1;
            *default_slot = default_range;
            maxrange = 1;
        } else {
            range.size--;
        }
    }
    
    clr_cmdext();
    add_cmdext("cmd",pCmdfn,"/C");

    for (i=0; i<crons.size; i++) {
        t_cronfile *cron = (t_cronfile *)array_get(&crons, i);
        if (LOGmode<0) printf("# crontab file %s\n",cron->fn);
        fp = fopen(cron->fn,"r");
        if (fp==NULL) {
            del_cronfile(cron->fn);
            i--;
        } else {
            while (fgets(buf,512,fp)!=0) {
                parse_cronline(buf);
            }
            fclose(fp);
        }
    }
    if (LOGmode<0) {
        printf("# Loaded %d cronjobs and %d time ranges\n",no_e,no_r);
    } else {
        LOGprintf("# Loaded %d cronjobs and %d time ranges\n",no_e,no_r);
        LOGprintf("- LOGmode=%d directory=%s\n",LOGmode,LOGinfodir);
        if (shutdowncmd[0]) LOGprintf("- shutdown cmd=%s\n",shutdowncmd);
        if (startupcmd[0]) LOGprintf("- startup  cmd=%s\n",startupcmd);
    }
}

void reload_crontab(void)
{
    struct stat curstat;
    int			i,sw=0;
    
    for (i=0; i<crons.size; i++) {
        t_cronfile *cron = (t_cronfile *)array_get(&crons, i);
        if (stat(cron->fn, &curstat)!=0) {
            del_cronfile(cron->fn);
            i--;
            sw=1;
        } else if (curstat.st_mtime != cron->stat.st_mtime) {
            sw=1;
        }
    }
    if (!sw) return;
    LOGprintf("- reload_crontab %s\n",pTabName);
    read_cronentries();
}

// ===================================== check and execution ===================================
int ExecProcess(int i, char *hashstr, char *cmd, char *params, char *outfn, int waitmsec)
{
    char cmdstr[256], *ptr;
    STARTUPINFO si;
    SECURITY_ATTRIBUTES sa;
    PROCESS_INFORMATION pi;
	HANDLE h; 
    BOOL ret = FALSE; 
    DWORD flags = CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS | CREATE_NEW_PROCESS_GROUP;

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;       

    if (i>=0) {
        cronentry *e = *(cronentry **)array_get(&entry, i);
        if (e && e->cmdstr) strcpy(cmdstr, e->cmdstr);
    } else {
        ptr = make_cmdstr(cmd,params);
        if (ptr==NULL) {
            LOGprintf("make_cmdstr failed for %s %s\n",cmd,params);
            return -1;
        }
        strcpy(cmdstr,ptr);
        free(ptr);
	}
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&si, sizeof(STARTUPINFO));

    h = CreateFile(outfn,
        GENERIC_ALL,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        &sa,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    si.cb = sizeof(STARTUPINFO); 
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = NULL;
    si.hStdError = h;
    si.hStdOutput = h;

    if (!CreateProcessA(NULL, cmdstr, NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi)) {
        LOGprintf("CreateProcess %s failed (%d).\n", outfn, GetLastError());
        noerr++;
        if (i>=0) {
            cronentry *e = *(cronentry **)array_get(&entry, i);
            e->hproc = 0;
            e->stim = time(0);
            e->pid = 0;
        }
        CloseHandle(h);
        return -1;
    }
    if (LOGmode>=3) LOGprintf("started %-5d pid=%-5d : %s %s\n",
                            pi.hProcess,pi.dwProcessId,hashstr,cmdstr);
    noexec++;
    noact++;
    if (i>=0) {
        cronentry *e = *(cronentry **)array_get(&entry, i);
        e->hproc = pi.hProcess;
        e->stim  = time(0);
        e->pid   = pi.dwProcessId;
	} else {
		DWORD rc;
		if (waitmsec==0) waitmsec=15000;
        rc = WaitForSingleObject(pi.hProcess, (DWORD)waitmsec);
        LOGprintf("finish  %-5d  rc=%-5d : %s %s\n", pi.hProcess,rc,hashstr,cmdstr);
        CloseHandle(pi.hProcess);
	    noact--;
    }
    CloseHandle(h);
    return 1;
}

void makelogfn(char *cmd, char *id, char *logfn)
{
    char bin[256], hashfn[256], *ptr;
    strcpy(bin,cmd);
    ptr = strrchr(bin,'\\');
    if (ptr==NULL) ptr = strrchr(bin,'/');
    ptr = (ptr) ? (ptr+1) : bin;
    sprintf(hashfn,"%s_%s_%s.info.txt",pBaseName,ptr,id);
    while ((ptr=strchr(hashfn,' ')) != NULL) *ptr='_';
    if (LOGinfodir[0]) {
        sprintf(logfn,"%s\\%s",LOGinfodir,hashfn);
    } else {
        strcpy(logfn,hashfn);
    }
}

int doexec(int i)
{
    int stat;
    cronentry *e = *(cronentry **)array_get(&entry, i);
    if (CONSOLEmode==2) {
        LOGprintf("Would have started %s\n",e->cmdstr);
        stat = 32;
    } else {
        char logfn[512], hashstr[9];
        if (e->hproc!=0) {
            DWORD result;
            int no=0;
            if (LOGmode>=2) LOGprintf("killproc %d for %s %s\n",e->pid,e->cmdstr);
            noact--;
            TerminateProcess(e->hproc, 0);
            do {
                result = WaitForSingleObject(e->hproc,50);
                no++;
            } while (no<20 && result == WAIT_TIMEOUT);
            CloseHandle(e->hproc);
            e->hproc = 0;
            e->pid = 0;
            e->stim = 0;
        }
        sprintf(hashstr,"%08x",e->hash);
        makelogfn(e->cmd,hashstr,logfn);
        stat = ExecProcess(i, hashstr,e->cmd,e->param,logfn,-1);
    }
    return stat;
}

DWORD doshellexecwait(char *cmdstr, char *paramstr, char *id, DWORD waitmsec)
{
    int stat;
    if (CONSOLEmode==2) {
        LOGprintf("Would have started %s %s\n",cmdstr,paramstr);
        stat = 32;
    } else {
        char logfn[512];
        makelogfn(cmdstr,id,logfn);
        stat = ExecProcess(-1,id,cmdstr,paramstr,logfn,waitmsec);
    }
    return stat;
}

int ismatch(char typ, int val, int fr, int un)
{
	int i;
    for (i=fr; i<=un; i++) {
        ranges *r = *(ranges **)array_get(&range, i);
        if (val>=r->low && val<=r->high) {
            if (r->step==1 || ((val-r->low)%r->step)==0) {
                if (LOGmode>=7) LOGprintf("ok time %2d %c  == %d-%d/%d\n",
                                        val,typ,r->low,r->high,r->step);
                return 1;
            }
        } 
        if (LOGmode>=7) LOGprintf("-- time %2d %c  != %d-%d/%d\n",
                                val,typ,r->low,r->high,r->step);
    }
    return 0;
}

int entry_match(struct tm *t, int idx)
{
    cronentry *e = *(cronentry **)array_get(&entry, idx);
    int stat;
    stat = ismatch('m',t->tm_min,e->m1,e->m2)
            && ismatch('h',t->tm_hour,e->h1,e->h2)
            && ismatch('d',t->tm_mday,e->d1,e->d2)
            && ismatch('M',t->tm_mon+1,e->M1,e->M2);
    if (stat) {
        if (t->tm_wday==0 || t->tm_wday==7) {
            if (ismatch('w',0,e->w1,e->w2) ||
                ismatch('w',7,e->w1,e->w2)) return 1;
        } else
            return ismatch('w',t->tm_wday,e->w1,e->w2);
    }
    return 0;
}

void check_cronjobs(void)
{
    struct tm *tc;
    int n;

    if (LOGmode>=4) LOGprintf("check running procs (%d)...\n",noact);
    tc = localtime(&now);
    for (n=0; n<entry.size; n++) { 
        cronentry *e = *(cronentry **)array_get(&entry, n);
        if (e->hproc!=0) {
            if (WaitForSingleObject(e->hproc,0)==WAIT_OBJECT_0) {
                time_t now = time(0);
                int dur = (int)(now-e->stim);
                char tmpstr[21];
                sprintf(tmpstr,"%ds",dur);
                if (LOGmode>=3) LOGprintf("finish  %-5d dur=%-5s : %08x %s\n",
                                        e->hproc,tmpstr,e->hash,e->cmdstr);
                CloseHandle(e->hproc);
                e->hproc = 0;
                e->pid = 0;
                noact--;
            }
        }
    }
}

void exe_cronjobs(void)
{
    struct tm *tc;
    int n, org_log;

    if (LOGmode>=4) LOGprintf("checking...\n");
    org_log = LOGmode;
    tc = localtime(&now);
    for (n=0; n<entry.size; n++) { 
        cronentry *e = *(cronentry **)array_get(&entry, n);
        LOGmode = org_log;
        if (e->log>org_log) LOGmode = e->log;
        if (entry_match(tc, n)!=0) 
            doexec(n);
        else if (LOGmode>=6)
            LOGprintf("nomatch %s %s\n", e->cmd, e->param);
    }
    LOGmode = org_log;
}

void doinit(void)
{
    // Initialize dynamic arrays
    array_init(&entry, sizeof(cronentry *));
    array_init(&range, sizeof(ranges *)   );
    array_init(&cmdext,sizeof(t_cmdext)   );
    array_init(&crons, sizeof(t_cronfile) );

    memset(&laststat,0,sizeof(laststat));
    now = time(0);
    st = localtime(&now);
}

void doinit_service(void)
{
	doinit();
    init_cronfile(pTabName);    
    reload_crontab();
	noexec = 0;
    noerr = 0;
    if (startupcmd[0]) {
        DWORD starttick = GetTickCount();
        LOGprintf("Startupcmd: %s\n",startupcmd);
        if (starttick < 900000) {
            doshellexecwait(startupcmd, "startall","startup",15000);
        } else {
            char *ptr = strchr(startupcmd,' ');
            if (ptr) *ptr++ = '\0';
            doshellexecwait(startupcmd, "start","start",15000);
        }
    }
}

DWORD WINAPI MyService(void)
{
    int waitmsec, cursec, lastsec;

    LOGprintf("Service %s started\n",pServiceName);
    SetEnvironmentVariable("cronservice",pServiceName);
    doinit_service();
    lastsec = 0;
    LOGprintf("exe_cronjobs\n");
    exe_cronjobs();
    while (1) {
        if (noact) check_cronjobs();
        if (StopServing) {
            LOGprintf("Service %s stopped\n",pServiceName);
            StoppedServing = 1;
            array_clear(&entry,  free_cronentry);
            array_clear(&range,  free_range);
            array_clear(&cmdext, free_cmdext);
            array_clear(&crons,  free_cronfile);
			Sleep(100);
            return 0;
        }
        now = time(0);
        cursec = now%60;
        if (cursec<lastsec) {
            exe_cronjobs();
            Sleep(1000);
            reload_crontab();
        } else if (cursec>=50 && cursec<57) reload_crontab();

        waitmsec = (60-(time(0)%60))*1000+200;
        if (waitmsec>5000) waitmsec = 5200;
        Sleep(waitmsec);
        lastsec = cursec;
    }
    return 0;
}