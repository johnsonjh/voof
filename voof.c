/*
 * voof.c -- public domain compressor shell...
 * ...but especially for Ross Williams' LZRW series.  USE AT YOUR OWN RISK.
 *
 * version 1:
 *     03 Aug 91
 *     mike gleason jr., NCEMRSoft.
 *     default algorithm: LZRW3-A
 *
 * version 1.1:
 *     07 Aug 91 -- mg
 *     + fixed dangerous bug with "long" pathnames.
 *     + changed name, previous name was a trademark of Apple Computer.
 *     + changed suffix accordingly. there were only four days between
 *       releases, so this shouldn't cause _too_ many problems!
 *     + added -f flag.
 *     + added disclaimers :-(
 */

#include <stdio.h>
#ifndef FILENAME_MAX				/* should be declared in "stdio.h" */
#	define FILENAME_MAX 12
#endif

#ifdef unix
#	include <sys/types.h>
#	include <sys/param.h>	/* should include sys/types.h, too, but... */
#	include <sys/stat.h>
#	include <sys/time.h>
#	include <unistd.h>
#else
#	include <time.h>
#	ifndef MAXPATHLEN
#		define MAXPATHLEN FILENAME_MAX
#	endif
#endif

#include <string.h>
#include "compress.h"				/* includes "port.h" */

#ifndef DEFBLOCKSIZE
#define DEFBLOCKSIZE 65536L			/* We need *2* of these */
#endif

#define SUFFIX			".V"		/* what .ext to tack on */
#define SUFFIX_STRLEN	((size_t) 2)	/* strlen(SUFFIX) */
typedef unsigned long FourByteType;	/* must be (atleast) 4 bytes */

#define FRead(ptr,n,fp) fread((char *)(ptr), (size_t) 1, (size_t) (n), (fp))
#define FWrite(ptr,n,fp) fwrite((char *)(ptr), (size_t) 1, (size_t) (n), (fp))

/* voof globals */
	clock_t						t0, t1;			/* start time, end time */
	int							delflag = 1;	/* remove old when finished? */
	int							verbose = 0;	/* print statistics? */
	int							overwrite = 0;	/* erase existing files? */
	int							zcatflag = 0;	/* leave compressed, dump output to stdout */
	int							action;			/* encoding or decoding? */
	ULONG						bytes_in;		/* bytes read from input file */
	ULONG						bytes_out;		/* bytes written to output file */
	ULONG						total_in;		/* bytes in, all files */
	ULONG						total_out;		/* bytes out, all files */
	size_t						blocksize;		/* size of i/o blocks */ 
	UBYTE						*inbuf = NULL;	/* input block */
	UBYTE						*outbuf = NULL;	/* output block */
	UBYTE						*workbuf = NULL;/* private mem for coder */
	FILE						*inf, *outf;	/* i/o files */
	struct compress_identity	*id = NULL;		/* info about algorithm */
	char						*infname;		/* filename to read */
	char						*outfname;		/* filename to write */
#ifdef unix
	struct stat					stbuf;
#endif


/* voof protos */
	int							do_decompress(), do_compress();
	int							Putl();
	FourByteType				Getl();


/* voof externs */
	void						*malloc();
	extern int					getopt(), optind;
	extern char					*optarg;
	extern char					*rindex();



main(argc, argv)
	int argc;
	char **argv;
{
	register int		(*compress_proc)();
	register int		flag, result;
	register char		*cp;
	clock_t				t2;
	int					more_than_one;
	
#ifdef THINK_C
#	include <console.h>
	argc = ccommand(&argv);
#endif

	action = COMPRESS_ACTION_COMPRESS;		/* compress by default. */
	
	if((cp = rindex(argv[0], '/')) != NULL) cp++;
	else cp = argv[0];
	
	if (*cp == 'v' && *++cp == 'c') {
		zcatflag = 1;  delflag = 0;			/* are we named "vcat?" */
		action = COMPRESS_ACTION_DECOMPRESS;
	} else if (*cp == 'f' || (*cp++ == 'u' && *cp == 'n'))
		/* are we named "foov" or "unvoof?" */
		action = COMPRESS_ACTION_DECOMPRESS;

	(void) compress(
		COMPRESS_ACTION_IDENTITY,			/* get algorithm id */
		NULL,
		NULL,
		0,
		NULL,
		(ULONG *) &id
	);
	blocksize = (size_t) DEFBLOCKSIZE;
	
	/*	Alloc space to hold pathnames.  These can be quite large, so
		use malloc instead of declaring them as char arrays[] */
	infname = (char *) malloc ((size_t) (MAXPATHLEN + SUFFIX_STRLEN + 2));
	outfname = (char *) malloc ((size_t) (MAXPATHLEN + SUFFIX_STRLEN + 2));
	if (infname == NULL || outfname == NULL) {
		perror("malloc");  exit (1);
	}
	
	while ((flag = getopt(argc, argv, "B:v^defFcCnH")) != EOF)
		switch(flag) {
			case 'B': blocksize = (size_t) atol(optarg); break;
			case 'v': verbose = 1; break;
			case '^': verbose = -1; break;	/* Dan Bernstein's idea */
			case 'd': action = COMPRESS_ACTION_DECOMPRESS; break;
			case 'e': action = COMPRESS_ACTION_COMPRESS; break;
			case 'F': /* or... */
			case 'f': overwrite = 1; break;
			case 'c':
				zcatflag = 1;
				delflag = 0;
				action = COMPRESS_ACTION_DECOMPRESS;
				break;
			case 'C':
				zcatflag = 1;
				delflag = 0;
				action = COMPRESS_ACTION_COMPRESS;
				break;
			case 'n': delflag = 0; break;
			default: usage(*argv);
		}
	
	if (action == COMPRESS_ACTION_DECOMPRESS) {
		compress_proc = do_decompress;
	} else {
		compress_proc = do_compress;
	}
	
	argv += optind; argc -= optind;
	more_than_one = (argc > 1);
	if (argc <= 0) {
		argc++;								/* no files specified, */
		*argv = NULL;						/* use stdin */
	}
	
	/* init workingmem that the algorithm needs  */
	workbuf = (UBYTE *) malloc ((size_t) (id)->memory);
	if (workbuf == NULL) {
		perror("malloc");
		exit(1);
	}
	
	t2 = clock();
	for (total_in = total_out = (ULONG) 0; argc-- > 0; argv++) {
		t0 = clock();						/* start timer */
		bytes_in = bytes_out = (ULONG) 0;	/* reset counters */
		result = (*compress_proc)(*argv);	/* encode or decode */
		t1 = clock();						/* stop timer */
		if (result == 0 && verbose != 0) {
			print_stats();
			total_in += bytes_in;
			total_out += bytes_out;
		}
	}
	
	if (verbose && more_than_one) {
		(void) strcpy(infname, "TOTAL"); (void) strcpy(outfname, "TOTAL");
		bytes_in = total_in;
		bytes_out = total_out;
		t0 = t2;
		print_stats();
	}
	
	exit (0);
}	/* main */




do_compress(fname)
	char *fname;
{
	register ULONG						insize;
	ULONG								outsize;
	register size_t						len;
	register char						*cp;
	
	/* buffers haven't been initialized yet. */
	inbuf = (UBYTE *) malloc ((size_t) blocksize);
	outbuf = (UBYTE *) malloc ((size_t) (blocksize + COMPRESS_OVERRUN));
	if (inbuf == NULL || outbuf == NULL) {
		perror("malloc");
		return (1);
	}
	
	
	/* open the input file */
	if (fname == NULL) {
		inf = stdin;	/* stdin to stdout */
		outf = stdout;
		*infname = *outfname = '\0';
	} else {
		(void) strcpy(infname, fname);

#ifdef unix
		if (stat(infname, &stbuf) == -1) {
			perror("stat");
			return (1);
		}
#endif
		/* Open it. */
		if ((inf = fopen (infname, FOPEN_BINARY_READ)) == NULL) {
			perror(infname);  return (1);
		}
		
		/* Open output file */
		if (zcatflag == 1) {
			outf = stdout;
			*outfname = '\0';
		} else {
			(void) strcpy(outfname, infname);
			if ((cp = rindex (outfname, '/')) != NULL)	cp++;
			else cp = outfname;
			len = strlen(cp);	/* length of simple filename */
			if (len + SUFFIX_STRLEN > FILENAME_MAX ||
				(len + strlen(outfname) + SUFFIX_STRLEN > MAXPATHLEN)) {
				fprintf(stderr,"%s: filename too long to tack on %s\n",cp);
				if (inf != stdin) (void) fclose (inf);
				return (1);
			}

			(void) strcat(outfname, SUFFIX);
			if (overwrite == 0 &&
#ifdef unix
				access(outfname, F_OK) == 0) {
#else
				(outf = fopen (outfname, "r")) != NULL) {
				fclose (outf);
#endif
				if (inf != stdin) (void) fclose (inf);
				(void) fprintf(stderr, "%s: will not overwrite.\n", outfname);
				return (1);
			}
			if ((outf = fopen (outfname, FOPEN_BINARY_WRITE)) == NULL) {
				if (inf != stdin) (void) fclose (inf);
				perror(outfname);  return (1);
			}
		}		
	}		/* not a null fname */


	/*	Write the id of the algorithm used, to 
		make sure that it can be decompressed later. */
	(void) Putl((FourByteType) (*id).id, outf);


	/*	Write out the size of the in buffer, which will be needed
		for decompressing some other time. */
	(void) Putl((FourByteType) blocksize, outf);


	/*	Write out a junk long as a pad, and maybe for future use. */
	(void) Putl((FourByteType) 0x564f4f46, outf);	/* 'VOOF' */
	bytes_out = (ULONG) 12;	/* we've already written 12 bytes */


	/*	Check errors quick before proceeding. */
	if (ferror(outf)) {
		perror("file error");
		return (1);
	}
	
	
	/* read in, mangle, then write out. */
	while ((insize = (ULONG) FRead(inbuf, blocksize, inf)) > (size_t)0) {		
		bytes_in += (ULONG) insize;
		(void) compress(
			COMPRESS_ACTION_COMPRESS,
			workbuf,
			inbuf,
			insize,
			outbuf,
			&outsize
		);
		/* Write the size of the compressed block, for l8r decompression */
		(void) Putl((FourByteType) outsize, outf);
		bytes_out += (ULONG) (outsize + 4);
		(void) FWrite (outbuf, outsize, outf);
	}
	
	close_files(1);
	return (0);
}	/* do_compress */




do_decompress(fname)
	char *fname;
{
	register ULONG						insize;
	ULONG								outsize;
	size_t								outbufsize;
	long								alg_id;
	
	/* open the input file */
	if (fname == NULL) {
		inf = stdin;		/* stdin to stdout */
		outf = stdout;
		*infname = *outfname = '\0';
	} else {
		(void) strcpy(infname, fname);
		(void) strcpy(outfname, fname);
		if (strcmp(fname + strlen(fname) - SUFFIX_STRLEN, SUFFIX) != 0) {
			/* No SUFFIX: tack one on */
			(void) strcat(infname, SUFFIX);
		} else
			outfname[strlen(outfname) - SUFFIX_STRLEN] = '\0';

		/*	Shouldn't need to worry about maximum file/pathnames here,
			since if "originalfile.V" is okay, then "originalfile" should
			work too. */
			
		/*	See if outfname already exists. */
		if (overwrite == 0 &&
#ifdef unix
			access(outfname, F_OK) == 0) {
#else
			(outf = fopen (outfname, "r")) != NULL) {
			fclose (outf);
#endif
			(void) fprintf(stderr, "%s: will not overwrite.\n", outfname);
			return (1);
		}
			
#ifdef unix
		if (stat(infname, &stbuf) == -1) {
			perror("stat");
			return (1);
		}
#endif
		/* Open it. */
		if ((inf = fopen (infname, FOPEN_BINARY_READ)) == NULL) {
			perror(infname);  return (1);
		}
	}		/* not a null fname */


	/* verify the input file's header. */
	alg_id = (long) Getl(inf);
	if ((*id).id != alg_id) {
		(void) fprintf (stderr,
"# %s: not a compressed file, or wrong algorithm.\n# ID was 0x%lx, it should be 0x%lx.\n",
			infname, alg_id, (*id).name);
		if (inf != stdin) (void) fclose (inf);
		return (1);
	}
	
	
	/*	Read in the size of the i/o blocks, which was written
		when it was compressed. */
	outbufsize = (size_t) Getl(inf);
	(void) Getl(inf);		/* pad longword */
	bytes_in = (ULONG) 12;	/* already read in 12 bytes */


	/*	Allocate mem for the output buffer */
	outbuf = (UBYTE *) malloc ((size_t) outbufsize);
	if (outbuf == NULL) {
		(void) fprintf(stderr, "%s: not enough memory to decompress.\n", infname);
		if (inf != stdin) (void) fclose (inf);
		return (1);
	}
	
	
	/*	Finally we're ready to open a output file. */
	if (fname != NULL) {
		/* Open output file */
		if (zcatflag == 1) {
			outf = stdout;
			*outfname = '\0';
		} else if ((outf = fopen (outfname, FOPEN_BINARY_WRITE)) == NULL) {
			if (inf != stdin) (void) fclose (inf);
			perror(outfname);  return (1);
		}		
	}		/* not a null fname */
	
	
	insize = (ULONG) Getl(inf);	/* Read in the size of the first block. */
	inbuf = (UBYTE *) malloc ((size_t) insize);
	if (inbuf == NULL) goto nomem;
	
	/* read in, mangle, then write out. */
	while ((insize = (ULONG) FRead(inbuf, insize, inf)) > 0) {		
		bytes_in += (ULONG) insize;
		(void) compress(
			COMPRESS_ACTION_DECOMPRESS,
			workbuf,
			inbuf,
			insize,
			outbuf,
			&outsize
		);
		bytes_out += (ULONG) outsize;
		(void) FWrite (outbuf, outsize, outf);
		insize = (ULONG) Getl(inf);	/* Read in the size of the next block. */
		free (inbuf); inbuf = NULL;
		if (insize == 0xffffffff || feof(inf))
			break;
		inbuf = (UBYTE *) malloc ((size_t) insize);
		if (inbuf == NULL) goto nomem;
	}
	
	close_files(1);	
	return (0);

nomem:
	(void) fprintf(stderr, "%s: not enough memory to decompress.\n", infname);
	close_files(0);
	return (1);
}	/* do_decompress */




close_files(ok)
	int ok;
{
	if (inbuf != NULL) free(inbuf);		inbuf = NULL;
	if (outbuf != NULL) free(outbuf);	outbuf = NULL;
	if (inf != stdin) {
		/* if 'inf' is a disk file */
		if (inf != NULL) { (void) fclose (inf); inf = NULL; }
		if (ok && delflag && unlink (infname) < 0)
				perror("delete");
	}
	
	if (outf != stdout) {
		/* if 'outf' is a disk file */
		if (outf != NULL) { (void) fclose (outf); outf = NULL; }
#ifdef unix
		if (ok)
			(void) chmod (outfname, stbuf.st_mode);
		else
#else
		if (!ok)
#endif
			(void) unlink (outfname);
	}
}	/* close_files */




#ifndef BIG_ENDIAN

/* Ugly code -- but seemingly portable, regardless of byte ordering */

FourByteType Getl (in)
	register FILE *in;
{
	register FourByteType aLongword;	/* 4 bytes */
	register FourByteType a, b, c, d;
	register long curpos;
	register int result;
		
	curpos = ftell(in);
	a = (FourByteType) getc (in) << 24;
	b = (FourByteType) getc (in) << 16;
	c = (FourByteType) getc (in) << 8;
	result = getc(in);
	if (result == EOF)
		return (0xffffffff);
	d = (FourByteType) result;
	aLongword = a + b + c + d;
	(void) fseek(in, curpos + 4L, 0);	/* make sure we are where we should be! */
	return (aLongword);
}	/* Getl */

int Putl (aLongword, out)
	register FourByteType aLongword;	/* 4 bytes */
	register FILE *out;
{
	register int a;	
	register long curpos;
	
	curpos = ftell(out);	
	a = (int) ((aLongword >> 24) & 0xff);  (void) putc(a, out);
	a = (int) ((aLongword >> 16) & 0xff);  (void) putc(a, out);
	a = (int) ((aLongword >> 8) & 0xff);  (void) putc(a, out);
	a = (int) (aLongword & 0xff);  (void) putc(a, out);
	a = ferror(out);
	if (a) return (EOF);
	(void) fseek(out, curpos + 4L, 0);	/* make sure we are where we should be! */	
	a = ferror(out) ? EOF : 0;
	return (a);
}	/* Putl */

#else

/*	If you know for sure the type of byte ordering your machine has
	is BIG_ENDIAN, you can #define it and you'll get a mild
	speed increase. */

FourByteType Getl (in)
	FILE *in;
{
	FourByteType aLongword;	/* 4 bytes */
	
	(void) fread ((char *)&aLongword, (size_t)4, (size_t)1, in);
	return (aLongword);
}	/* Getl */

int Putl (aLongword, out)
	FourByteType aLongword;	/* 4 bytes */
	FILE *out;
{
	(void) fwrite((char *)&aLongword, (size_t)4, (size_t)1, out);
	return (ferror(out) ? EOF : 0);
}	/* Putl */

#endif




print_stats()
{
	register REAL				ratio, elapsed_time;
	register char				*cp;
	
	elapsed_time = ((REAL) (t1 - t0)) / ((REAL) CLOCKS_PER_SEC);
	
	if (action == COMPRESS_ACTION_COMPRESS) {
		/* print compression stats */
		
		if (bytes_in > 0)
			ratio = (100.0 * bytes_out) / bytes_in;
		else ratio = 0.0;
		
		if (*infname == '\0') strcpy(infname, "stdin");
		if ((cp = rindex (infname, '/')) != NULL) cp++;
		else cp = infname;
		
		(void) fprintf(stderr,
"%-12s -> %lu in  %lu out  %.2f%%  %.2f sec  %.2f K/s  %.2f bits\n",
			cp,
			bytes_in,
			bytes_out,
			(bytes_in <= 0 ? 0 : (verbose>1 ? 100.0 - ratio : ratio)),
			elapsed_time,
			((REAL) bytes_in / 1024.0) / elapsed_time,
			(REAL) (0.08 * ratio)
		);
	} else {
		/* print decompression stats */
		
		if (*outfname == '\0') strcpy(outfname, "stdout");
		if ((cp = rindex (outfname, '/')) != NULL) cp++;
		else cp = outfname;

		(void) fprintf(stderr,
"%-12s -> in: %lu  out: %lu  %.2f sec  %.2f K/s\n",
			cp,
			bytes_in,
			bytes_out,
			elapsed_time,
			((REAL) bytes_in / 1024.0) / elapsed_time
		);
	}
}	/* print_stats */




usage(progname)
	char *progname;
{
	(void) fprintf(stderr,
"%s by Mike Gleason Jr., NCEMRSoft... USE AT YOUR OWN RISK.\n\
\tUsage: %s [-cCdefHnv^] [-B size] filelist\n\
\t-c : decompress to stdout, do not delete original (like zcat)\n\
\t-C : compress to stdout, do not delete orginal (same as '%s <file')\n\
\t-d : decode compressed file\n\
\t-e : encode file (on by default)\n\
\t-f : force to overwrite existing files\n\
\t-H : help (this screen)\n\
\t-n : do not delete original file\n\
\t-v : (verbose) print statistics, use %% of original\n\
\t-^ : same as above, except express %% as %%saved\n\
\t-B : size of i/o blocks (compresssion only; default is %ld)\n",
	progname, progname, progname, DEFBLOCKSIZE);

	if (id != NULL)
		(void) fprintf(stderr, 
"\nAlgorithm Information:\n\
\tName :              %s\n\
\tVersion :           %s\n\
\tDate :              %s\n\
\tAuthor :            %s\n\
\tCopyright :         %s\n\
\tAffiliation :       %s\n\
\tVendor :            %s\n\
\tID Number :         0x%lx\n\
\tMemory Required :   %lu\n", 
	(*id).name,
	(*id).version,
	(*id).date,
	(*id).author,
	(*id).copyright,
	(*id).affiliation,
	(*id).vendor,
	(*id).id,
	(*id).memory);
		
	exit (1);
}	/* usage */

/* eof */
