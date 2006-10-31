#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#define M 6
#define M2 (1 << M)
#define successor fingers[1].n
#define PSSIZE 400
#define NUMNODES 6

typedef struct finger finger;
typedef struct node node;

typedef int ID;

struct finger {
  ID start;
  ID end;
  node *n;
};

struct node {
  finger fingers[M+1];
  node *predecessor;
  ID id;
};

/* Chord protocol implementation */

int inrangegtle(ID id, ID start, ID end)
{
  if (end > start)
    return (id > start) && (id <= end);
  else
    return (id > start) || (id <= end);
}

int inrangegtlt(ID id, ID start, ID end)
{
  if (end > start)
    return (id > start) && (id < end);
  else
    return (id > start) || (id < end);
}

node *closest_preceding_finger(node *n, ID id)
{
  int i;
  for (i = M; 1 <= i; i--)
    if (inrangegtlt(n->fingers[i].n->id,n->id,id))
      return n->fingers[i].n;
  return n;
}

node *find_predecessor(node *n, ID id)
{
  node *ndash = n;
  while (!inrangegtle(id,ndash->id,ndash->successor->id))
    ndash = closest_preceding_finger(ndash,id);
  return ndash;
}

node *find_successor(node *n, ID id)
{
  node *ndash = find_predecessor(n,id);
  return ndash->successor;
}

void join(node *n, node *ndash)
{
  n->predecessor = NULL;
  n->successor = find_successor(ndash,n->id);
}

void notify(node *n, node *ndash)
{
  if (!n->predecessor || inrangegtlt(ndash->id,n->predecessor->id,n->id))
    n->predecessor = ndash;
}

void stabilize(node *n)
{
  node *x = n->successor->predecessor;
  if (x && inrangegtlt(x->id,n->id,n->successor->id))
    n->successor = x;
  notify(n->successor,n);
}

void fix_fingers(node *n)
{
  int i = 2+(rand()%(M-2));
  n->fingers[i].n = find_successor(n,n->fingers[i].start);
}

void fix_all_fingers(node *n)
{
  int i;
  for (i = 2; i <= M; i++)
    n->fingers[i].n = find_successor(n,n->fingers[i].start);
}

/* Node initialisation */

node *addnode(ID id)
{
  node *n = (node*)calloc(1,sizeof(node));
  int k;
  n->id = id;
  n->predecessor = NULL;
  for (k = 1; k <= M; k++) {
    n->fingers[k].start = (n->id + (int)pow(2,k-1))%M2;
    n->fingers[k].n = n;
  }
  for (k = 1; k < M; k++)
    n->fingers[k].end = n->fingers[k+1].start;
  n->fingers[M].end = n->id;
  return n;
}

node **addnodes(int count)
{
  node **nodes = (node**)malloc(count*sizeof(node**));
  int i;
  int j;
  for (i = 0; i < count; i++) {
    ID id;
    int unique;
    do {
      unique = 1;
      id = rand()%M2;;
      for (j = 0; j < i; j++) {
        if (nodes[j]->id == id) {
          unique = 0;
          printf("rand(): id %d is already in use\n",id);
        }
      }
    } while (!unique);

    nodes[i] = addnode(id);
  }
  return nodes;
}

void print_node(node *n)
{
  printf("node %-6d ",n->id);
  if (n->successor == n)
    printf("successor (self)");
  else
    printf("successor %-6d",n->successor->id);

  if (!n->predecessor)
    printf("predecessor (none)");
  else if (n->predecessor == n)
    printf("predecessor (self)");
  else
    printf("predecessor %-6d",n->predecessor->id);
  printf("\n");

  int k;
  for (k = 1; k <= M; k++) {
    char interval[100];
    sprintf(interval,"[%d,%d)",n->fingers[k].start,n->fingers[k].end);
    printf("    finger %-2d interval %-10s",k,interval);
    printf(" d %-6d",(M2+(n->fingers[k].end-n->fingers[k].start)%M2)%M2);
    printf(" node %d\n",n->fingers[k].n->id);
  }
}

/* Postscript output */

void getpoint(ID id, double radius, double *x, double *y)
{
  double total = (double)M2;
  double pos = ((double)id)/total;
  double angle = 2.0*M_PI*pos-0.5*M_PI;
  *x = PSSIZE/2+cos(angle)*(double)radius;
  *y = PSSIZE/2-sin(angle)*(double)radius;
}

void printps(node **nodes, int count)
{
  FILE *f;
  int radius = 3*PSSIZE/8;
  int k;
  int g;

  if (NULL == (f = fopen("out.ps","w"))) {
    perror("out.ps");
    exit(-1);
  }

  fprintf(f,"%%!PS-Adobe-2.0 EPSF-2.0\n");
  fprintf(f,"%%%%BoundingBox: 0 0 %d %d\n",PSSIZE,PSSIZE);
  fprintf(f,"%%%%EndComments\n");
  fprintf(f,"/Times-Roman findfont\n");
  fprintf(f,"14 scalefont\n");
  fprintf(f,"setfont\n");
  fprintf(f,"\n");
  fprintf(f,"newpath\n");
  fprintf(f,"%d %d %d 0 360 arc\n",PSSIZE/2,PSSIZE/2,radius);
  fprintf(f,"stroke\n");
  fprintf(f,"\n");

  printf("sin(90) = %f\n",sin(90));

  for (k = 0; k < count; k++) {
    double x, y, tx, ty;
    getpoint(nodes[k]->id,radius,&x,&y);
    getpoint(nodes[k]->id,radius*1.1,&tx,&ty);
    fprintf(f,"newpath\n");
    fprintf(f,"%f %f 5 0 360 arc\n",x,y);
    fprintf(f,"fill\n");
    fprintf(f,"\n");
    fprintf(f,"newpath\n");
    fprintf(f,"%f %f moveto\n",tx,ty);
    fprintf(f,"(%d) stringwidth\n",nodes[k]->id);
    fprintf(f,"/yval exch def\n");
    fprintf(f,"/xval exch def\n");
    fprintf(f,"xval -2 div -5 rmoveto\n");
    fprintf(f,"(%d) show\n",nodes[k]->id);
  }

  k = 0; /* node to draw finger lines for */
  fprintf(f,"gsave\n");
  fprintf(f,"0.7 0.7 0.7 setrgbcolor\n");
  fprintf(f,"[3 3] 0 setdash\n");
  for (g = 1; g <= M; g++) {
    double nx, ny, fx, fy;
    getpoint(nodes[k]->id,radius,&nx,&ny);
    getpoint(nodes[k]->fingers[g].start,radius,&fx,&fy);
    fprintf(f,"newpath\n");
    fprintf(f,"%f %f moveto\n",nx,ny);
    fprintf(f,"%f %f lineto\n",fx,fy);
    fprintf(f,"stroke\n");
    fprintf(f,"\n");
  }
  fprintf(f,"grestore\n");

  fclose(f);
}

int main(int argc, char **argv)
{
  node **nodes;
  int i;
  int s;
  ID id;

  setbuf(stdout,NULL);
  srand(0);

  printf("M = %d\n",M);
  printf("2^M = %d\n",M2);

  nodes = addnodes(NUMNODES);

  for (i = 0; i < NUMNODES; i++)
    print_node(nodes[i]);

  printps(nodes,NUMNODES);


  for (i = 1; i < NUMNODES; i++) {
    printf("joining %d\n",i);
    join(nodes[i],nodes[0]);
  }

  printf("----------------- after join -----------------\n");
  for (i = 0; i < NUMNODES; i++)
    print_node(nodes[i]);

  for (s = 0; s < 10; s++) {
    for (i = 0; i < NUMNODES; i++) {
      stabilize(nodes[i]);
      fix_all_fingers(nodes[i]);
    }

    printf("----------------- after stabilization %d -----------------\n",s);
    for (i = 0; i < NUMNODES; i++)
      print_node(nodes[i]);
  }

  for (id = 0; id < M2; id++) {
    node *n = find_successor(nodes[0],id);
    printf("successor of %d is %d\n",id,n->id);
  }

  return 0;
}
