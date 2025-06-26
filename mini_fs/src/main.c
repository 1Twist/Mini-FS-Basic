/*─────────────────────────────────────────────────────────────*/
/*  Mini-shell para o mini-filesystem in-memory                */
/*─────────────────────────────────────────────────────────────*/
#include "fs.h"
#include "auth.h"
#include "directory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_CMD 256

/*── prompt ───────────────────────────────────────────────────*/
static void prompt(void)
{
    char path[256];
    dir_pwd(path, sizeof path);
    printf("%s$ ", path);
    fflush(stdout);
}

/*── echo "txt" >|>> arq  ─────────────────────────────────────*/
static int do_echo(char *line)
{
    char *q1 = strchr(line, '"');  if (!q1) return -1;
    char *q2 = strchr(q1 + 1, '"');if (!q2) return -1;
    *q2 = '\0';
    const char *txt = q1 + 1;

    char *p = q2 + 1; while (*p==' ') ++p;
    int append = 0;
    if (!strncmp(p, ">>", 2)) { append = 1; p += 2; }
    else if (*p == '>')       { append = 0; ++p; }
    else return -1;
    while (*p==' ') ++p;

    return *p ? fs_echo(p, txt, append) : -1;
}
static int parse_two(const char *s, char *a, char *b)
{ return sscanf(s, "%63s %63s", a, b) == 2 ? 0 : -1; }

/*── re-autenticação do admin quando solicitada ───────────────*/
static int admin_reauth(void)
{
    char pass[64];
    printf("Senha admin: "); fgets(pass, sizeof pass, stdin);
    pass[strcspn(pass, "\n")] = '\0';
    User *adm = auth_get_user("admin");
    return (adm && strcmp(adm->passwd, pass) == 0) ? 0 : -1;
}

/*── "rwx" → bits ─────────────────────────────────────────────*/
static int rwx_to_bits(const char *s)
{
    if (strlen(s)!=3) return -1;
    int b = 0;
    if (s[0]=='r') b |= 4; else if (s[0]!='-') return -1;
    if (s[1]=='w') b |= 2; else if (s[1]!='-') return -1;
    if (s[2]=='x') b |= 1; else if (s[2]!='-') return -1;
    return b;
}

/*── help contextual ─────────────────────────────────────────*/
static void show_help(void)
{
    puts("Comandos principais");
    puts("  pwd | ls [-l] | mkdir <dir> | cd <dir>");
    puts("  touch <arq>");
    puts("  echo \"txt\" > arq     ou   echo \"txt\" >> arq");
    puts("  cat <arq> | rm <arq> | cp <orig> <dest> | mv <orig> <dest>");
    puts("");
    puts("Gerenciamento de grupo / perfil");
    puts("  joingroup <grp>       (pede senha admin)");
    puts("  sg <gid>              (muda GID efetivo)");
    puts("  setperm <owner|group|public> rwx   (pede admin)");
    puts("");
    puts("Sessão");
    puts("  logout   | exit");
    puts("");

    if (auth_is_admin()) {
        puts("ADMIN (root)");
        puts("  useradd  <nome> <uid> <gid> <senha> <perm-oct>");
        puts("  userdel  <nome>");
        puts("  groupadd <nome> <gid> <perm-oct>");
        puts("  su <nome>");
        puts("  chmod <octal> <arq>");
        puts("  save");
        puts("");
    }
}

/*─────────────────────────────────────────────────────────────*/
int main(void)
{
    auth_init();   /* carrega users.db / groups.db */
    fs_init();     /* inicia bloco + diretórios    */
    if (!auth_login()) return 0;

    char line[MAX_CMD];

    while (1) {
        prompt();
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (!*line) continue;

        /*── exit / logout ───────────────────────────────────*/
        if (!strcmp(line,"exit")) { auth_save(); break; }

        if (!strcmp(line,"logout")) {
            auth_logout(); auth_save();
            if (!auth_login()) break;
            continue;
        }

        /*── bloco ADMIN (uid-0) ─────────────────────────────*/
        if (auth_is_admin()) {
            if (!strncmp(line,"useradd ",8)) {
                char u[32], pw[32]; unsigned uid,gid,perm;
                if (sscanf(line+8,"%31s %u %u %31s %o",
                           u,&uid,&gid,pw,&perm)==5)
                     puts(auth_useradd(u,uid,gid,pw,(uint16_t)perm)
                          ?"falha":"ok");
                else puts("Uso: useradd <n> <uid> <gid> <pw> <perm>");
                continue;
            }
            if (!strncmp(line,"userdel ",8)) {
                char u[32];
                if (sscanf(line+8,"%31s",u)==1)
                     puts(auth_delete_user(u)?"falha":"removido");
                else puts("Uso: userdel <nome>");
                auth_save(); continue;
            }
            if (!strncmp(line,"groupadd ",9)) {
                char g[32]; unsigned gid,perm;
                if (sscanf(line+9,"%31s %u %o",g,&gid,&perm)==3)
                     puts(auth_groupadd(g,gid,(uint16_t)perm)
                          ?"falha":"ok");
                else puts("Uso: groupadd <n> <gid> <perm>");
                auth_save(); continue;
            }
            if (!strncmp(line,"su ",3)) {
                User *u = auth_get_user(line+3);
                if (u){ auth_set_uid(u->uid); auth_set_gid(u->gid); }
                else   puts("usuário inexistente");
                continue;
            }
            if (!strncmp(line,"chmod ",6)) {
                unsigned p; char f[64];
                if (sscanf(line+6,"%o %63s",&p,f)==2)
                     puts(fs_chmod(f,(uint16_t)p)?"falha":"ok");
                else puts("Uso: chmod <octal> <arquivo>");
                continue;
            }
            if (!strcmp(line,"save")){
                puts(auth_save()?"falha":"BD salvo");
                continue;
            }
        }

        /*── joingroup ───────────────────────────────────────*/
        if (!strncmp(line,"joingroup ",10)) {
            char g[32];
            if (sscanf(line+10,"%31s",g)!=1){ puts("Uso: joingroup <grp>"); continue; }
            Group *grp = auth_get_group(g);
            if (!grp){ puts("grupo inexistente"); continue; }
            if (admin_reauth()){ puts("senha incorreta"); continue; }

            User *me = auth_get_user_by_uid(auth_uid());
            if (auth_add_user_to_group(me->name,g)==0){
                auth_set_gid(grp->gid);
                puts("adicionado ao grupo");
                auth_save();
            } else puts("falha joingroup");
            continue;
        }

        /*── setperm ─────────────────────────────────────────*/
        if (!strncmp(line,"setperm ",8)) {
            char cls[8],mask[4];
            if (sscanf(line+8,"%7s %3s",cls,mask)!=2){
                puts("Uso: setperm <owner|group|public> rwx"); continue;
            }
            int bits=rwx_to_bits(mask); if(bits<0){ puts("másc inválida"); continue; }
            int sh = !strcmp(cls,"owner")?6:!strcmp(cls,"group")?3:
                     !strcmp(cls,"public")?0:-1;
            if (sh<0){ puts("classe inválida"); continue; }
            if (admin_reauth()){ puts("senha incorreta"); continue; }

            User *me=auth_get_user_by_uid(auth_uid());
            me->dflt_perms=(me->dflt_perms & ~(7<<sh)) | (bits<<sh);
            printf("Perm padrão → %03o\n",me->dflt_perms);
            auth_save(); continue;
        }

        /*── sg ──────────────────────────────────────────────*/
        if (!strncmp(line,"sg ",3)){
            auth_set_gid((uint32_t)atoi(line+3));
            continue;
        }

        /*── Arquivos / diretórios ───────────────────────────*/
        if (!strcmp(line,"pwd")){
            char p[256]; puts(dir_pwd(p,sizeof p)); continue;
        }
        if (!strncmp(line,"ls",2)){
            dir_ls(strstr(line,"-l")!=NULL); continue;
        }
        if (!strncmp(line,"mkdir ",6)){
            if (dir_mkdir(line+6)) puts("mkdir: permissão negada");
            continue;
        }
        if (!strncmp(line,"cd ",3)){
            if (dir_cd(line+3))
                puts("cd: permissão ou caminho inválido");
            continue;
        }
        if (!strncmp(line,"touch ",6)){
            if (fs_touch(line+6)) puts("touch: permissão negada");
            continue;
        }

        if (!strncmp(line,"echo ",5)){
            if (do_echo(line+5)<0)
                puts("Uso: echo \"txt\" > arq  ou  echo \"txt\" >> arq");
            continue;
        }
        if (!strncmp(line,"cat ",4)){
            if (fs_cat(line+4)) puts("cat: permissão negada");
            continue;
        }
        if (!strncmp(line,"rm ",3)){
            if (fs_rm(line+3)) puts("rm: permissão negada");
            continue;
        }
        if (!strncmp(line,"cp ",3)){
            char a[64],b[64];
            if (parse_two(line+3,a,b)==0){
                if (fs_cp(a,b)) puts("cp: erro/permissão");
            }else puts("Uso: cp <orig> <dest>");
            continue;
        }
        if (!strncmp(line,"mv ",3)){
            char a[64],b[64];
            if (parse_two(line+3,a,b)==0){
                if (fs_mv(a,b)) puts("mv: erro/permissão");
            }else puts("Uso: mv <orig> <dest>");
            continue;
        }

        /* help */
        if (!strcmp(line,"help")) { show_help(); continue; }

        puts("Comando desconhecido — digite help");
    }
    return 0;
}
