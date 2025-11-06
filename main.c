#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PLAYERS 4
#define MAX_NAME    24
#define MAX_HISTORY 200

typedef struct {
	int players;
	int commander; // 0: standard (20 life), 1: commander (40 life)
	int life[MAX_PLAYERS];
	int poison[MAX_PLAYERS];
	int cmdmg[MAX_PLAYERS][MAX_PLAYERS];
	int turn;
} State;

typedef struct {
	State hist[MAX_HISTORY];
	int top;
	int cur;
} History;

/* ------------------ Utility Functions ------------------ */
static void copy_state(State *dst, const State *src) {
	*dst = *src;
}
static State* cur(History *H) {
	return &H->hist[H->cur];
}

static void hist_init(History *H, const State *initial) {
	H->top = H->cur = 0;
	H->hist[0] = *initial;
}

static void hist_push(History *H, const State *next) {
	if (H->cur < H->top) H->top = H->cur;
	if (H->top + 1 >= MAX_HISTORY) {
		for(int i=0; i<H->top; i++) H->hist[i] = H->hist[i+1];
		H->top--;
		if (H->cur > 0) H->cur--;
	}
	H->top++;
	H->cur++;
	H->hist[H->cur] = *next;
}

static int hist_undo(History *H) {
	if (H->cur == 0) return 0;
	H->cur--;
	return 1;
}
static int hist_redo(History *H) {
	if (H->cur == H->top) return 0;
	H->cur++;
	return 1;
}

static void reset_match(State *S, int players, int commander) {
	memset(S, 0, sizeof(*S));
	if(players < 2) players = 2;
	if(players > MAX_PLAYERS) players = MAX_PLAYERS;
	S->players = players;
	S->commander = commander;
	for(int i=0; i<players; i++) {
		S->life[i] = commander ? 40 : 20;
		S->poison[i] = 0;
	}
	S->turn = 0;
}

/* ------------------ Display ------------------ */

static void show(const State *S) {
	puts("\n--------------------------------------------------");
	printf("Players: %d | Mode: %s | Turn: P%d\n",
	       S->players, S->commander ? "Commander (40 life)" : "Constructed (20 life)", S->turn + 1);
	puts("Idx  Life  Poison   | Commander Damage (to P_i from P_j)");
	for(int i=0; i<S->players; i++) {
		printf("P%-3d %-5d %-7d | ", i+1, S->life[i], S->poison[i]);
		if(S->commander) {
			for(int j=0; j<S->players; j++) {
				if(i==j) continue;
				if(S->cmdmg[i][j] > 0)
					printf("P%d:%d ", j+1, S->cmdmg[i][j]);
			}
		}
		puts("");
	}
	puts("--------------------------------------------------");
}

static void print_help(void) {
	puts("\nMagic: The Gathering Life Tracker Commands:");
	puts("--------------------------------------------------");
	puts("  new <players 2-6> [c]        Start new game; add 'c' for Commander (40 life)");
	puts("  +<p> <n> / -<p> <n>          Add/subtract life for player p  (e.g. +1 3)");
	puts("  set <p> <n>                  Set life of player p");
	puts("  poison <p> <+/-n>            Add/remove poison counters");
	puts("  cmd <target> <source> <+n>   Commander dmg to <target> from <source>");
	puts("  next                         Pass turn to next player");
	puts("  show                         Display life totals");
	puts("  roll [dN]                    Roll a die (default d20, e.g. roll d6)");
	puts("  coin                         Flip a coin");
	puts("  undo / redo                  Undo or redo last action");
	puts("  help                         Show this help text");
	puts("  quit                         Exit program");
	puts("--------------------------------------------------");
}

/* ------------------ Random Helpers ------------------ */
static int rintn(int n) {
	return (rand() % n) + 1;
}
static int parse_die(const char *s) {
	return (s && (s[0]=='d'||s[0]=='D')) ? atoi(s+1) : 20;
}

/* ------------------ Main Program ------------------ */
int main(void) {
	srand((unsigned)time(NULL));

	State S;
	reset_match(&S, 4, 1);
	History H;
	hist_init(&H, &S);

	puts("==================================================");
	puts("Magic: The Gathering Life / Poison Tracker");
	puts("==================================================");
	print_help();  // show the command list immediately
	show(cur(&H));

	char line[256];
	while(1) {
		printf("\n(Type 'help' for commands)\n> ");
		if(!fgets(line, sizeof(line), stdin)) break;

		size_t n = strlen(line);
		if(n && (line[n-1]=='\n' || line[n-1]=='\r')) line[n-1] = '\0';
		if(line[0]=='\0') continue;

		State T = *cur(&H);
		int changed = 0;

		/* --- Command handling --- */
		if(!strncmp(line,"quit",4)) break;
		else if(!strncmp(line,"help",4)) {
			print_help();
			continue;
		}
		else if(!strncmp(line,"show",4)) {
			show(&T);
			continue;
		}
		else if(!strncmp(line,"undo",4)) {
			if(hist_undo(&H)) show(cur(&H));
			else puts("Nothing to undo.");
			continue;
		}
		else if(!strncmp(line,"redo",4)) {
			if(hist_redo(&H)) show(cur(&H));
			else puts("Nothing to redo.");
			continue;
		}
		else if(!strncmp(line,"next",4)) {
			T.turn = (T.turn + 1) % T.players;
			changed = 1;
		}
		else if(!strncmp(line,"coin",4)) {
			printf("You flipped: %s\n", (rand() & 1) ? "Heads" : "Tails");
			continue;
		}
		else if(!strncmp(line,"roll",4)) {
			char d[16]="";
			int N=20;
			if(sscanf(line+4, " %15s", d)==1) N=parse_die(d);
			printf("Rolled d%d: %d\n", N, rintn(N));
			continue;
		}
		else if(!strncmp(line,"new",3)) {
			int p=0;
			char c[8]="";
			int got = sscanf(line+3, " %d %7s", &p, c);
			int cmd = (got==2 && (c[0]=='c'||c[0]=='C')) ? 1 : 0;
			if(p<=0) p=4;
			reset_match(&T, p, cmd);
			changed=1;
		}
		else if(!strncmp(line,"set",3)) {
			int p,val;
			if(sscanf(line+3," %d %d",&p,&val)==2) {
				if(p>=1 && p<=T.players) {
					T.life[p-1]=val;
					changed=1;
				}
			}
		}
		else if(!strncmp(line,"poison",6)) {
			int p,dv;
			if(sscanf(line+6," %d %d",&p,&dv)==2) {
				if(p>=1 && p<=T.players) {
					T.poison[p-1]+=dv;
					if(T.poison[p-1]<0) T.poison[p-1]=0;
					changed=1;
				}
			}
		}
		else if(!strncmp(line,"cmd",3)) {
			int tgt,src,inc;
			if(sscanf(line+3," %d %d %d",&tgt,&src,&inc)==3) {
				if(T.commander && tgt>=1 && tgt<=T.players && src>=1 && src<=T.players && tgt!=src) {
					T.cmdmg[tgt-1][src-1]+=inc;
					if(T.cmdmg[tgt-1][src-1]<0) T.cmdmg[tgt-1][src-1]=0;
					changed=1;
				} else puts("Usage: cmd <target> <source> <+n>");
			}
		}
		else if(line[0]=='+' || line[0]=='-') {
			int p,dv;
			if(sscanf(line+1," %d %d",&p,&dv)==2) {
				if(p>=1 && p<=T.players) {
					if(line[0]=='-') dv=-dv;
					T.life[p-1]+=dv;
					changed=1;
				}
			}
		}
		else {
			puts("Unknown command. Type 'help' for a list of valid inputs.");
			continue;
		}

		if(changed) {
			hist_push(&H, &T);
			show(cur(&H));
		}
	}

	puts("\nThanks for playing!");
	return 0;
}

