/* Last changed Time-stamp: <2007-12-05 13:55:42 ronny> */
/*
		  Ineractive Access to folding Routines

		  c Ivo L Hofacker
		  Vienna RNA package
*/

/** \file **/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include "fold.h"
#include "part_func.h"
#include "fold_vars.h"
#include "PS_dot.h"
#include "utils.h"
#include "RNAfold_cmdl.h"
extern void  read_parameter_file(const char fname[]);
extern float circfold(const char *string, char *structure);
extern plist * stackProb(double cutoff);
/*@unused@*/
static char UNUSED rcsid[] = "$Id: RNAfold.c,v 1.25 2009/02/24 14:22:21 ivo Exp $";

#define PRIVATE static

static char  scale1[] = "....,....1....,....2....,....3....,....4";
static char  scale2[] = "....,....5....,....6....,....7....,....8";

PRIVATE struct plist *b2plist(const char *struc);
PRIVATE struct plist *make_plist(int length, double pmin);

/*--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
  struct  gengetopt_args_info args_info;
  char    *string, *line, *structure=NULL, *cstruc=NULL;
  char    fname[13], ffname[20], gfname[20], *ParamFile=NULL;
  char    *ns_bases=NULL, *c;
  int     i, length, l, sym, r, istty, pf=0, noPS=0, noconv=0, circ=0;
  double  energy, min_en, kT, sfact=1.07;

  do_backtrack  = 1;
  string        = NULL;

  /*
  #############################################
  # check the command line prameters
  #############################################
  */
  if(cmdline_parser (argc, argv, &args_info) != 0) exit(1);
  /* temperature */
  if(args_info.temp_given)        temperature = args_info.temp_arg;
  /* structure constraint */
  if(args_info.constraint_given)  fold_constrained=1;
  /* do not take special tetra loop energies into account */
  if(args_info.noTetra_given)     tetra_loop=0;
  /* set dangle model */
  if(args_info.dangles_given)     dangles = args_info.dangles_arg;
  /* do not allow weak pairs */
  if(args_info.noLP_given)        noLonelyPairs = 1;
  /* do not allow wobble pairs (GU) */
  if(args_info.noGU_given)        noGU = 1;
  /* do not allow weak closing pairs (AU,GU) */
  if(args_info.noClosingGU_given) no_closingGU = 1;
  /* do not convert DNA nucleotide "T" to appropriate RNA "U" */
  if(args_info.noconv_given)      noconv = 1;
  /* set energy model */
  if(args_info.energyModel_given) energy_set = args_info.energyModel_arg;
  /* take another energy parameter set */
  if(args_info.paramFile_given)   ParamFile = strdup(args_info.paramFile_arg);
  /* Allow other pairs in addition to the usual AU,GC,and GU pairs */
  if(args_info.nsp_given)         ns_bases = strdup(args_info.nsp_arg);
  /* set pf scaling factor */
  if(args_info.pfScale_given)     sfact = args_info.pfScale_arg;
  /* assume RNA sequence to be circular */
  if(args_info.circ_given)        circ=1;
  /* do not produce postscript output */
  if(args_info.noPS_given)        noPS=1;
  /* partition function settings */
  if(args_info.partfunc_given){
    pf = 1;
    if(args_info.partfunc_arg != -1)
      do_backtrack = args_info.partfunc_arg;
  }

  /* free allocated memory of command line data structure */
  cmdline_parser_free (&args_info);

  if (ParamFile != NULL)
    read_parameter_file(ParamFile);

  if (circ && noLonelyPairs)
    fprintf(stderr, "warning, depending on the origin of the circular sequence, some structures may be missed when using -noLP\nTry rotating your sequence a few times\n");

  if (ns_bases != NULL) {
    nonstandards = space(33);
    c=ns_bases;
    i=sym=0;
    if (*c=='-') {
      sym=1; c++;
    }
    while (*c!='\0') {
      if (*c!=',') {
	nonstandards[i++]=*c++;
	nonstandards[i++]=*c;
	if ((sym)&&(*c!=*(c-1))) {
	  nonstandards[i++]=*c;
	  nonstandards[i++]=*(c-1);
	}
      }
      c++;
    }
  }
  istty = isatty(fileno(stdout))&&isatty(fileno(stdin));
  if ((fold_constrained)&&(istty)) {
    printf("Input constraints using the following notation:\n");
    printf("| : paired with another base\n");
    printf(". : no constraint at all\n");
    printf("x : base must not pair\n");
    printf("< : base i is paired with a base j<i\n");
    printf("> : base i is paired with a base j>i\n");
    printf("matching brackets ( ): base i pairs base j\n");
  }

  do {				/* main loop: continue until end of file */
    if (istty) {
      printf("\nInput string (upper or lower case); @ to quit\n");
      printf("%s%s\n", scale1, scale2);
    }
    fname[0]='\0';
    if ((line = get_line(stdin))==NULL) break;

    /* skip comment lines and get filenames */
    while ((*line=='*')||(*line=='\0')||(*line=='>')) {
      if (*line=='>')
	(void) sscanf(line, ">%12s", fname);
      printf("%s\n", line);
      free(line);
      if ((line = get_line(stdin))==NULL) break;
    }

    if ((line ==NULL) || (strcmp(line, "@") == 0)) break;

    string = (char *) space(strlen(line)+1);
    (void) sscanf(line,"%s",string);
    free(line);
    length = (int) strlen(string);

    structure = (char *) space((unsigned) length+1);
    if (fold_constrained) {
      cstruc = get_line(stdin);
      if (cstruc!=NULL)
	strncpy(structure, cstruc, length);
      else
	fprintf(stderr, "constraints missing\n");
    }
    for (l = 0; l < length; l++) {
      string[l] = toupper(string[l]);
      if (!noconv && string[l] == 'T') string[l] = 'U';
    }
    if (istty)
      printf("length = %d\n", length);

    /* initialize_fold(length); */
    if (circ)
      min_en = circfold(string, structure);
    else
      min_en = fold(string, structure);
    printf("%s\n%s", string, structure);
    if (istty)
      printf("\n minimum free energy = %6.2f kcal/mol\n", min_en);
    else
      printf(" (%6.2f)\n", min_en);

    (void) fflush(stdout);

    if (fname[0]!='\0') {
      strcpy(ffname, fname);
      strcat(ffname, "_ss.ps");
      strcpy(gfname, fname);
      strcat(gfname, "_ss.g");
    } else {
      strcpy(ffname, "rna.ps");
      strcpy(gfname, "rna.g");
    }
    if (!noPS) (void) PS_rna_plot(string, structure, ffname);
    if (length>2000) free_arrays(); 
    if (pf) {
      char *pf_struc;
      pf_struc = (char *) space((unsigned) length+1);
	if (dangles==1) {
	  dangles=2;   /* recompute with dangles as in pf_fold() */
	  min_en = (circ) ? energy_of_circ_struct(string, structure) : energy_of_struct(string, structure);
	  dangles=1;
      }

      kT = (temperature+273.15)*1.98717/1000.; /* in Kcal */
      pf_scale = exp(-(sfact*min_en)/kT/length);
      if (length>2000) fprintf(stderr, "scaling factor %f\n", pf_scale);

      (circ) ? init_pf_circ_fold(length) : init_pf_fold(length);

      if (cstruc!=NULL)
	strncpy(pf_struc, cstruc, length+1);
      energy = (circ) ? pf_circ_fold(string, pf_struc) : pf_fold(string, pf_struc);

      if (do_backtrack) {
	printf("%s", pf_struc);
	if (!istty) printf(" [%6.2f]\n", energy);
	else printf("\n");
      }
      if ((istty)||(!do_backtrack))
	printf(" free energy of ensemble = %6.2f kcal/mol\n", energy);
      if (do_backtrack) {
	plist *pl1,*pl2;
	char *cent;
	double dist, cent_en;
	cent = centroid(length, &dist);
	cent_en = (circ) ? energy_of_circ_struct(string, cent) :energy_of_struct(string, cent);
	printf("%s {%6.2f d=%.2f}\n", cent, cent_en, dist);
	free(cent);
	if (fname[0]!='\0') {
	  strcpy(ffname, fname);
	  strcat(ffname, "_dp.ps");
	} else strcpy(ffname, "dot.ps");
	pl1 = make_plist(length, 1e-5);
	pl2 = b2plist(structure);
	(void) PS_dot_plot_list(string, ffname, pl1, pl2, "");
	free(pl2);
	if (do_backtrack==2) {
	  pl2 = stackProb(1e-5);
	  if (fname[0]!='\0') {
	    strcpy(ffname, fname);
	    strcat(ffname, "_dp2.ps");
	  } else strcpy(ffname, "dot2.ps");
	  PS_dot_plot_list(string, ffname, pl1, pl2,
			   "Probabilities for stacked pairs (i,j)(i+1,j-1)");
	  free(pl2);
	}
	free(pl1);
	free(pf_struc);
      }
      printf(" frequency of mfe structure in ensemble %g; ",
	     exp((energy-min_en)/kT));
      if (do_backtrack)
	printf("ensemble diversity %-6.2f", mean_bp_dist(length));

      printf("\n");
      free_pf_arrays();

    }
    if (cstruc!=NULL) free(cstruc);
    (void) fflush(stdout);
    free(string);
    free(structure);
  } while (1);
  return 0;
}

PRIVATE struct plist *b2plist(const char *struc) {
  /* convert bracket string to plist */
  short *pt;
  struct plist *pl;
  int i,k=0;
  pt = make_pair_table(struc);
  pl = (struct plist *)space(strlen(struc)/2*sizeof(struct plist));
  for (i=1; i<strlen(struc); i++) {
    if (pt[i]>i) {
      pl[k].i = i;
      pl[k].j = pt[i];
      pl[k++].p = 0.95*0.95;
    }
  }
  free(pt);
  pl[k].i=0;
  pl[k].j=0;
  pl[k++].p=0.;
  return pl;
}


PRIVATE struct plist *make_plist(int length, double pmin) {
  /* convert matrix of pair probs to plist */
  struct plist *pl;
  int i,j,k=0,maxl;
  maxl = 2*length;
  pl = (struct plist *)space(maxl*sizeof(struct plist));
  k=0;
  for (i=1; i<length; i++)
    for (j=i+1; j<=length; j++) {
      if (pr[iindx[i]-j]<pmin) continue;
      if (k>=maxl-1) {
	maxl *= 2;
	pl = (struct plist *)xrealloc(pl,maxl*sizeof(struct plist));
      }
      pl[k].i = i;
      pl[k].j = j;
      pl[k++].p = pr[iindx[i]-j];
    }
  pl[k].i=0;
  pl[k].j=0;
  pl[k++].p=0.;
  return pl;
}

