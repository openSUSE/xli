/* #ident	"@(#)x11:contrib/clients/xloadimage/buildshar.c 1.5 93/07/23 Labtam" */
/* buildshar.c:
 *
 * utility program to figure out how to pack shar files.  it doesn't try
 * very hard.
 *
 * this program is in the public domain.
 *
 * jim frost 09.25.90
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* this defines the size of the shar file not including anything that got
 * added by shar.  should be considerably smaller than 64000.
 */

#define SHAR_SIZE 40000

struct file {
  char *name;
  int size;
  struct file *next;
};

struct shar {
  struct file *file;
  struct shar *next;
};

main(int argc, char **argv)
{ int num_files, files_left;
  struct file *files;
  struct stat sbuf;
  struct shar *shar_list;
  struct shar *cur_shar;
  struct file *cur_file;
  int shar_size, shar_num, num_shars;
  int a;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s [files]\n", *argv);
  }

  /* build table of filenames
   */

  files= (struct file *)malloc(sizeof(struct file) * (argc - 1));
  for (a= 0, num_files= 0; *(++argv); a++) {
    if (stat(*argv, &sbuf) < 0) {
      perror(*argv);
      files[a].name= NULL;
      continue;
    }
    num_files++;
    files[a].name= *argv;
    files[a].size= sbuf.st_size;
  }

  /* try to fit files.  this is not a bright algorithm.
   */

  shar_list= cur_shar= (struct shar *)malloc(sizeof(struct shar));
  cur_shar->file= NULL;
  cur_file= NULL;
  shar_size= 0;

  for (files_left= num_files; files_left;) {

    /* look for a file that'll fit in the current shar
     */

    for (a= 0; a < num_files; a++)
      if (files[a].name &&
	  ((shar_size + files[a].size <= SHAR_SIZE) ||
	   ((shar_size == 0) && (files[a].size > SHAR_SIZE)))) {
	shar_size += files[a].size;
	if (cur_file) {
	  cur_file->next= (struct file *)malloc(sizeof(struct file));
	  cur_file= cur_file->next;
	}
	else
	  cur_shar->file= cur_file= (struct file *)malloc(sizeof(struct file));
	cur_file->name= files[a].name;
	cur_file->next= NULL;
	files[a].name= NULL;
	files_left--;
	break;
      }

    /* if nothing fit, make a new shar file
     */

    if (a == num_files) {
      cur_shar->next= (struct shar *)malloc(sizeof(struct shar));
      cur_shar= cur_shar->next;
      cur_shar->file= NULL;
      cur_shar->next= NULL;
      cur_file= NULL;
      shar_size= 0;
    }
  }

  /* find out how many shar files we need for -e flag
   */

  for (num_shars= 0, cur_shar= shar_list; cur_shar; num_shars++)
    cur_shar= cur_shar->next;

  /* output each shar command line
   */

  for (shar_num= 1, cur_shar= shar_list; cur_shar; shar_num++) {
    printf("shar -n %d -e %d", shar_num, num_shars);
    for (cur_file= cur_shar->file; cur_file; cur_file= cur_file->next)
      printf(" %s", cur_file->name);
    cur_shar= cur_shar->next;
    printf(" > shar.%d\n", shar_num);
  }
  exit(0);
}
