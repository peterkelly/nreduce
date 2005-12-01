#include "util/String.h"
#include "Reducer.h"
#include <argp.h>
#include <string.h>
#include <errno.h>

using namespace GridXSLT;

static char doc[] =
  "ml";

static char args_doc[] = "FILENAME\n";

static struct argp_option options[] = {
  {"dot",                      'd', "FILE",  0, "Output graph in dot format to FILE" },
  {"trace",                    't', NULL,    0, "Enable tracing" },
  { 0 }
};

struct arguments {
  char *filename;
  char *dot;
  bool trace;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
  struct arguments *arguments = (struct arguments*)state->input;

  switch (key) {
  case 'd':
    arguments->dot = arg;
    break;
  case 't':
    arguments->trace = true;
    break;
  case ARGP_KEY_ARG:
    if (0 == state->arg_num)
      arguments->filename = arg;
    else
      argp_usage (state);
    break;
  case ARGP_KEY_END:
    if (1 > state->arg_num)
      argp_usage (state);
    break;
  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

extern int mldebug;

int main(int argc, char **argv)
{
  struct arguments arguments;

  setbuf(stdout,NULL);
//  mldebug = 1;

  memset(&arguments,0,sizeof(arguments));
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  char buf[1024];
  int r;
  FILE *f;
  if (NULL == (f = fopen(arguments.filename,"r"))) {
    perror(arguments.dot);
    return 1;
  }

  stringbuf *input = stringbuf_new();
  while (0 < (r = fread(buf,1,1024,f)))
    stringbuf_append(input,buf,r);

  fclose(f);

  String code(input->data);
  stringbuf_free(input);

  Graph *g = parseGraph(code);

  if (NULL == g)
    return 1;

  if (NULL != arguments.dot) {
    FILE *dotfile;
    if (NULL == (dotfile = fopen(arguments.dot,"w"))) {
      perror(arguments.dot);
      return 1;
    }
    g->outputDot(dotfile,true);
    fclose(dotfile);
  }

//   g->outputTree(stdout);
//   message("---------------\n");
  g->trace = arguments.trace;
  g->numberCells();
  g->outputStep(" - start");
  while (g->reduce(g->root)) {
    // keep reducing
  }
  g->outputTree(stdout);
//  message("total reductions: %d\n",g->reduction);
  delete g;

  return 0;
}

