#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <GL/glew.h>
#include <GL/freeglut.h>

#undef USE_SRGB

#ifdef USE_SRGB
#define COMP_FMT	GL_COMPRESSED_SRGB8_ETC2
#else
#define COMP_FMT	GL_COMPRESSED_RGB8_ETC2
#endif

int init(void);
int texcomp(unsigned char *compix, unsigned int tofmt, unsigned char *pixels,
		int xsz, int ysz, unsigned int fromfmt, unsigned int fromtype);
void disp(void);
void reshape(int x, int y);
void keyb(unsigned char key, int x, int y);
void idle();
void gen_image(unsigned char *pixels, int xsz, int ysz);
unsigned char *load_compressed_image(const char *fname, int *cszptr, int *xszptr, int *yszptr);
int dump_compressed_image(const char *fname, unsigned char *data, int size, int w, int h);
void print_compressed_formats(void);

unsigned int tex, tex2, comp_tex;
const char *texfile;
int subtest, copytest, loop;

int main(int argc, char **argv)
{
	int i;
	unsigned int glut_flags = GLUT_RGB | GLUT_DOUBLE;
#ifdef USE_SRGB
	glut_flags |= GLUT_SRGB;
#endif

	for(i=1; i<argc; i++) {
		if(argv[i][0] == '-') {
			if(strcmp(argv[i], "-subtest") == 0) {
				subtest = 1;
			} else if(strcmp(argv[i], "-copytest") == 0) {
				copytest = 1;
			} else if(strcmp(argv[i], "-copytest-loop") == 0) {
				copytest = 1;
				loop = 1;
			} else {
				fprintf(stderr, "invalid option: %s\n", argv[i]);
				return 1;
			}
		} else {
			if(texfile) {
				fprintf(stderr, "unexpected argument: %s\n", argv[i]);
				return 1;
			}
			texfile = argv[i];
		}
	}

	glutInit(&argc, argv);
	glutInitWindowSize(800, 600);
	glutInitDisplayMode(glut_flags);
	glutCreateWindow("test");

	glutDisplayFunc(disp);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keyb);

	if (loop)
		glutIdleFunc(idle);

	glewInit();

	if(init() == -1) {
		return 1;
	}

	glutMainLoop();
	return 0;
}

int init(void)
{
	unsigned char *pixels, *buf;
	int xsz = 512;
	int ysz = 512;
	int is_comp = 0;
	int comp_size = 0;
	int tmp;

	print_compressed_formats();

	if(texfile) {
		if(!(pixels = load_compressed_image(texfile, &comp_size, &xsz, &ysz))) {
			return -1;
		}
		printf("loaded compressed texture file: %s (%dx%d)\n", texfile, xsz, ysz);

	} else {
		if(!(pixels = malloc(xsz * ysz * 3))) {
			abort();
		}
		gen_image(pixels, xsz, ysz);

		printf("compressing texture\n");
		if((comp_size = texcomp(pixels, COMP_FMT, pixels, xsz, ysz, GL_RGB, GL_UNSIGNED_BYTE)) == -1) {
			return -1;
		}
		printf("compressed texture is %d bytes (uncompressed was: %d)\n", comp_size, xsz * ysz * 3);

		dump_compressed_image("compressed_texture", pixels, comp_size, xsz, ysz);
	}

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glCompressedTexImage2D(GL_TEXTURE_2D, 0, COMP_FMT, xsz, ysz, 0, comp_size, pixels);
	if(glGetError()) {
		fprintf(stderr, "failed to upload compressed texture\n");
		return -1;
	}

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &is_comp);
	if(!is_comp) {
		fprintf(stderr, "texture is not compressed\n");
		return -1;
	}
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &tmp);
	if(tmp != comp_size) {
		fprintf(stderr, "internal compressed size differs (expected: %d, got: %d)!\n", comp_size, tmp);
		return -1;
	}

	if(!(buf = malloc(comp_size))) {
		fprintf(stderr, "failed to allocate comparison image buffer (%d bytes)\n", comp_size);
		return -1;
	}
	glGetCompressedTexImage(GL_TEXTURE_2D, 0, buf);

	if(memcmp(pixels, buf, comp_size) != 0) {
		fprintf(stderr, "submitted and retrieved pixel data differ!\n");
	} else {
		printf("submitted and retrieved sizes match (%d bytes)\n", comp_size);
	}

	if(subtest) {
		printf("testing glGetCompressedTextureSubImage and glCompressedTexSubImage2D\n");
		memset(buf, 0, comp_size);
		glGetCompressedTextureSubImage(tex, 0, 192, 64, 0, 64, 64, 1, comp_size, buf);
		glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 32, 32, 64, 64, COMP_FMT, 2048, buf);
	}

	if(copytest) {
		printf("testing glCopyImageSubData\n");

		glGenTextures(1, &tex2);
		glBindTexture(GL_TEXTURE_2D, tex2);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, COMP_FMT, xsz, ysz, 0, comp_size, pixels);
		glBindTexture(GL_TEXTURE_2D, 0);

		glCopyImageSubData(tex2, GL_TEXTURE_2D, 0, 128, 64, 0,
				tex, GL_TEXTURE_2D, 0, 32, 32, 0, 64, 64, 1);

		glBindTexture(GL_TEXTURE_2D, tex);
	}

	free(buf);
	free(pixels);

#ifdef USE_SRGB
	glEnable(GL_FRAMEBUFFER_SRGB);
#endif

	glEnable(GL_TEXTURE_2D);
	return 0;
}

int texcomp(unsigned char *compix, unsigned int tofmt, unsigned char *pixels,
		int xsz, int ysz, unsigned int fromfmt, unsigned int fromtype)
{
	unsigned int tex;
	int is_comp = 0;
	int comp_size = 0;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, tofmt, xsz, ysz, 0, fromfmt, fromtype, pixels);
	if(glGetError()) {
		fprintf(stderr, "failed to compress texture\n");
		return -1;
	}

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &is_comp);
	if(!is_comp) {
		fprintf(stderr, "texture is not compressed\n");
		return -1;
	}
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &comp_size);

	glGetCompressedTexImage(GL_TEXTURE_2D, 0, compix);
	return comp_size;
}

void disp(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex2f(-1, -1);
	glTexCoord2f(1, 1); glVertex2f(1, -1);
	glTexCoord2f(1, 0); glVertex2f(1, 1);
	glTexCoord2f(0, 0);	glVertex2f(-1, 1);
	glEnd();

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

void reshape(int x, int y)
{
	float aspect = (float)x / (float)y;
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glScalef(1.0 / aspect, 1, 1);
}

void keyb(unsigned char key, int x, int y)
{
	if(key == 27) {
		exit(0);
	}
}

void idle()
{
	glutPostRedisplay();
}

void gen_image(unsigned char *pixels, int xsz, int ysz)
{
	int i, j;

	for(i=0; i<ysz; i++) {
		for(j=0; j<xsz; j++) {
			int xor = i ^ j;

			*pixels++ = xor & 0xff;
			*pixels++ = (xor << 1) & 0xff;
			*pixels++ = (xor << 2) & 0xff;
		}
	}
}

unsigned char *load_compressed_image(const char *fname, int *cszptr, int *xszptr, int *yszptr)
{
	unsigned char *pixels;
	long start, comp_size;
	int xsz, ysz;
	FILE *fp;

	if(!(fp = fopen(texfile, "rb"))) {
		fprintf(stderr, "failed to open compressed texture file: %s: %s\n", texfile, strerror(errno));
		return 0;
	}

	if(fread(&xsz, sizeof xsz, 1, fp) < 1 || fread(&ysz, sizeof ysz, 1, fp) < 1) {
		fprintf(stderr, "failed to read compressed texture file header: %s: %s\n", texfile, strerror(errno));
		fclose(fp);
		return 0;
	}
	start = ftell(fp);
	fseek(fp, 0, SEEK_END);
	comp_size = ftell(fp) - start;
	fseek(fp, start, SEEK_SET);

	if(!(pixels = malloc(comp_size))) {
		perror("failed to allocate pixel buffer");
		return 0;
	}
	if(fread(pixels, 1, comp_size, fp) < comp_size) {
		fprintf(stderr, "failed to read compressed texture file: %s: %s\n", texfile, strerror(errno));
		fclose(fp);
		free(pixels);
		return 0;
	}
	fclose(fp);

	*cszptr = comp_size;
	*xszptr = xsz;
	*yszptr = ysz;
	return pixels;
}

int dump_compressed_image(const char *fname, unsigned char *data, int size, int w, int h)
{
	FILE *fp;

	if(!(fp = fopen(fname, "wb"))) {
		fprintf(stderr, "failed to open compressed texture dump file: %s: %s\n", fname, strerror(errno));
		return -1;
	}

	if(fwrite(&w, sizeof w, 1, fp) < 1 ||
			fwrite(&h, sizeof h, 1, fp) < 1 ||
			fwrite(data, 1, size, fp) < size) {
		fprintf(stderr, "failed to dump compressed texture: %s\n", strerror(errno));
	}
	fclose(fp);
	return 0;
}

const char *fmtstr(int fmt)
{
	switch(fmt) {
	case 0x86b0: return "GL_COMPRESSED_RGB_FXT1_3DFX";
	case 0x86b1: return "GL_COMPRESSED_RGBA_FXT1_3DFX";
	case 0x8dbb: return "GL_COMPRESSED_RED_RGTC1";
	case 0x8dbc: return "GL_COMPRESSED_SIGNED_RED_RGTC1";
	case 0x8dbd: return "GL_COMPRESSED_RG_RGTC2";
	case 0x8dbe: return "GL_COMPRESSED_SIGNED_RG_RGTC2";
	case 0x8e8c: return "GL_COMPRESSED_RGBA_BPTC_UNORM";
	case 0x8e8d: return "GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM";
	case 0x8e8e: return "GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT";
	case 0x8e8f: return "GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT";
	case 0x9274: return "GL_COMPRESSED_RGB8_ETC2";
	case 0x9275: return "GL_COMPRESSED_SRGB8_ETC2";
	case 0x9276: return "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2";
	case 0x9277: return "GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2";
	case 0x9278: return "GL_COMPRESSED_RGBA8_ETC2_EAC";
	case 0x9279: return "GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC";
	case 0x9270: return "GL_COMPRESSED_R11_EAC";
	case 0x9271: return "GL_COMPRESSED_SIGNED_R11_EAC";
	case 0x9272: return "GL_COMPRESSED_RG11_EAC";
	case 0x9273: return "GL_COMPRESSED_SIGNED_RG11_EAC";
	case 0x83F0: return "GL_COMPRESSED_RGB_S3TC_DXT1_EXT";
	case 0x83F1: return "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT";
	case 0x83F2: return "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT";
	case 0x83F3: return "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT";
	case 0x8C48: return "GL_COMPRESSED_SRGB_EXT";
	case 0x8C49: return "GL_COMPRESSED_SRGB_ALPHA_EXT";
	case 0x8C4A: return "GL_COMPRESSED_SLUMINANCE_EXT";
	case 0x8C4B: return "GL_COMPRESSED_SLUMINANCE_ALPHA_EXT";
	case 0x8C4C: return "GL_COMPRESSED_SRGB_S3TC_DXT1_EXT";
	case 0x8C4D: return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT";
	case 0x8C4E: return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT";
	case 0x8C4F: return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT";
	case 0x8B90: return "GL_PALETTE4_RGB8_OES";
	case 0x8B91: return "GL_PALETTE4_RGBA8_OES";
	case 0x8B92: return "GL_PALETTE4_R5_G6_B5_OES";
	case 0x8B93: return "GL_PALETTE4_RGBA4_OES";
	case 0x8B94: return "GL_PALETTE4_RGB5_A1_OES";
	case 0x8B95: return "GL_PALETTE8_RGB8_OES";
	case 0x8B96: return "GL_PALETTE8_RGBA8_OES";
	case 0x8B97: return "GL_PALETTE8_R5_G6_B5_OES";
	case 0x8B98: return "GL_PALETTE8_RGBA4_OES";
	case 0x8B99: return "GL_PALETTE8_RGB5_A1_OES";
	case 0x93B0: return "GL_COMPRESSED_RGBA_ASTC_4";
	case 0x93B1: return "GL_COMPRESSED_RGBA_ASTC_5";
	case 0x93B2: return "GL_COMPRESSED_RGBA_ASTC_5";
	case 0x93B3: return "GL_COMPRESSED_RGBA_ASTC_6";
	case 0x93B4: return "GL_COMPRESSED_RGBA_ASTC_6";
	case 0x93B5: return "GL_COMPRESSED_RGBA_ASTC_8";
	case 0x93B6: return "GL_COMPRESSED_RGBA_ASTC_8";
	case 0x93B7: return "GL_COMPRESSED_RGBA_ASTC_8";
	case 0x93B8: return "GL_COMPRESSED_RGBA_ASTC_10";
	case 0x93B9: return "GL_COMPRESSED_RGBA_ASTC_10";
	case 0x93BA: return "GL_COMPRESSED_RGBA_ASTC_10";
	case 0x93BB: return "GL_COMPRESSED_RGBA_ASTC_10";
	case 0x93BC: return "GL_COMPRESSED_RGBA_ASTC_12";
	case 0x93BD: return "GL_COMPRESSED_RGBA_ASTC_12";
	case 0x93D0: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4";
	case 0x93D1: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5";
	case 0x93D2: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5";
	case 0x93D3: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6";
	case 0x93D4: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6";
	case 0x93D5: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8";
	case 0x93D6: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8";
	case 0x93D7: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8";
	case 0x93D8: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10";
	case 0x93D9: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10";
	case 0x93DA: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10";
	case 0x93DB: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10";
	case 0x93DC: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12";
	case 0x93DD: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12";
	default:
		break;
	}
	return "unknown";
}

void print_compressed_formats(void)
{
	int i, num_fmt;
	int *fmtlist;

	glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &num_fmt);
	printf("%d generic compressed texture formats available:\n", num_fmt);

	if(!(fmtlist = malloc(num_fmt * sizeof *fmtlist))) {
		fprintf(stderr, "failed to allocate texture formats enumeration buffer\n");
		return;
	}
	glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, fmtlist);

	for(i=0; i<num_fmt; i++) {
		printf(" %05x: %s ", fmtlist[i], fmtstr(fmtlist[i]));
		GLint params;
		glGetInternalformativ(GL_TEXTURE_2D, fmtlist[i], GL_TEXTURE_COMPRESSED, 1, &params);
		printf("(%s format)\n", params == GL_TRUE ? "compressed" : "not compressed");
	}
	free(fmtlist);
}
