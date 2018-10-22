#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <GL/glew.h>
#include <GL/freeglut.h>

struct texture {
	unsigned int id;
	int width, height;
	unsigned int fmt;
	unsigned int compsize;
	void *data;
};

int init(void);
void disp(void);
void reshape(int x, int y);
void keyb(unsigned char key, int x, int y);
void idle(void);
int load_texture(const char *fname, struct texture *tex);
const char *fmtstr(int fmt);
void print_compressed_formats(void);

struct texture tex;
unsigned int tex2;
const char *texfile;
int subtest, copytest;

int main(int argc, char **argv)
{
	int i, loop = 0;
	unsigned int glut_flags = GLUT_RGB | GLUT_DOUBLE;

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

	if(!texfile) {
		fprintf(stderr, "you must specify a compressed texture file\n");
		return 1;
	}

	glutInit(&argc, argv);
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
	unsigned char *buf;
	int is_comp = 0;
	int tmp;
	unsigned int intfmt;

	print_compressed_formats();

	if(load_texture(texfile, &tex) == -1) {
		fprintf(stderr, "failed to load texture %s\n", texfile);
		return -1;
	}

	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, (int*)&intfmt);
	if(intfmt != tex.fmt) {
		fprintf(stderr, "internal format differs (expected: %s [%x], got: %s [%x])\n",
				fmtstr(tex.fmt), tex.fmt, fmtstr(intfmt), intfmt);
		return -1;
	}
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED, &is_comp);
	if(!is_comp) {
		fprintf(stderr, "texture is not compressed\n");
		return -1;
	}
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &tmp);
	if(tmp != tex.compsize) {
		fprintf(stderr, "internal compressed size differs (expected: %d, got: %d)!\n", tex.compsize, tmp);
		return -1;
	}

	if(!(buf = malloc(tex.compsize))) {
		fprintf(stderr, "failed to allocate comparison image buffer (%d bytes)\n", tex.compsize);
		return -1;
	}
	glGetCompressedTexImage(GL_TEXTURE_2D, 0, buf);

	if(memcmp(tex.data, buf, tex.compsize) != 0) {
		int i;
		unsigned char *a = tex.data, *b = buf;

		for(i=0; i<tex.compsize; i++) {
			if(*a++ != *b++) break;
		}
		assert(i < tex.compsize);
		fprintf(stderr, "submitted and retrieved pixel data differ! (at offset %d)\n", i);
	} else {
		printf("submitted and retrieved sizes match (%d bytes)\n", tex.compsize);
	}

	if(subtest) {
		printf("testing glGetCompressedTextureSubImage and glCompressedTexSubImage2D\n");
		memset(buf, 0, tex.compsize);
		glGetCompressedTextureSubImage(tex.id, 0, 192, 64, 0, 64, 64, 1, tex.compsize, buf);
		glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 32, 32, 64, 64, tex.fmt, 2048, buf);
	}

	if(copytest) {
		printf("testing glCopyImageSubData\n");

		glGenTextures(1, &tex2);
		glBindTexture(GL_TEXTURE_2D, tex2);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, tex.fmt, tex.width, tex.height, 0, tex.compsize, tex.data);
		glBindTexture(GL_TEXTURE_2D, 0);

		glCopyImageSubData(tex2, GL_TEXTURE_2D, 0, 128, 64, 0,
				tex.id, GL_TEXTURE_2D, 0, 32, 32, 0, 64, 64, 1);

		glBindTexture(GL_TEXTURE_2D, tex.id);
	}

	free(buf);

	glEnable(GL_TEXTURE_2D);
	return 0;
}

void disp(void)
{
	int x = 0, y = 0;
	int xsz = tex.width;
	int ysz = tex.height;

	glClear(GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D, tex.id);
	glEnable(GL_TEXTURE_2D);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 1); glVertex2f(0, 0);
	glTexCoord2f(1, 1); glVertex2f(xsz, 0);
	glTexCoord2f(1, 0); glVertex2f(xsz, ysz);
	glTexCoord2f(0, 0);	glVertex2f(0, ysz);

	x += xsz;
	xsz /= 2;
	ysz /= 2;

	while(xsz & ysz) {
		glTexCoord2f(0, 1); glVertex2f(x, y);
		glTexCoord2f(1, 1); glVertex2f(x + xsz, y);
		glTexCoord2f(1, 0); glVertex2f(x + xsz, y + ysz);
		glTexCoord2f(0, 0);	glVertex2f(x, y + ysz);

		y += ysz;

		xsz /= 2;
		ysz /= 2;
	}
	glEnd();

	glutSwapBuffers();
	assert(glGetError() == GL_NO_ERROR);
}

void reshape(int x, int y)
{
	glViewport(0, 0, x, y);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, x, 0, y, -1, 1);
}

void keyb(unsigned char key, int x, int y)
{
	if(key == 27) {
		exit(0);
	}
}

void idle(void)
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

struct header {
	char magic[8];
	uint32_t glfmt;
	uint16_t flags;
	uint16_t levels;
	uint32_t width, height;
	struct {
		uint32_t offset, size;
	} datadesc[20];
	char unused[8];
};

int load_texture(const char *fname, struct texture *tex)
{
	int i;
	FILE *fp;
	struct header hdr;
	void *buf;

	if(!(fp = fopen(fname, "rb"))) {
		fprintf(stderr, "failed to open file: %s: %s\n", fname, strerror(errno));
		return -1;
	}
	if(fread(&hdr, 1, sizeof hdr, fp) != sizeof hdr) {
		fprintf(stderr, "failed to read image file header: %s: %s\n", fname, strerror(errno));
		fclose(fp);
		return -1;
	}
	if(memcmp(hdr.magic, "COMPTEX0", sizeof hdr.magic) != 0 || hdr.levels < 0 ||
			hdr.levels > 20 || !hdr.datadesc[0].size) {
		fprintf(stderr, "%s is not a compressed texture file, or is corrupted\n", fname);
		fclose(fp);
		return -1;
	}

	if(!(buf = malloc(hdr.datadesc[0].size))) {
		fprintf(stderr, "failed to allocate compressed texture buffer (%d bytes): %s\n",
				hdr.datadesc[0].size, strerror(errno));
		fclose(fp);
		return -1;
	}
	if(!(tex->data = malloc(hdr.datadesc[0].size))) {
		fprintf(stderr, "failed to allocate data buffer\n");
		fclose(fp);
		free(buf);
		return -1;
	}


	glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			hdr.levels > 1 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	tex->fmt = hdr.glfmt;
	tex->width = hdr.width;
	tex->height = hdr.height;
	tex->compsize = hdr.datadesc[0].size;

	printf("%s: %dx%d format: %s\n", fname, tex->width, tex->height, fmtstr(tex->fmt));
	glutReshapeWindow(tex->width + tex->width / 2, tex->height);

	for(i=0; i<hdr.levels; i++) {
		if(!hdr.datadesc[i].size) {
			continue;
		}

		if(fread(buf, 1, hdr.datadesc[i].size, fp) != hdr.datadesc[i].size) {
			fprintf(stderr, "unexpected EOF while reading texture: %s\n", fname);
			fclose(fp);
			free(buf);
			glDeleteTextures(1, &tex->id);
			return -1;
		}
		if(i == 0) {
			memcpy(tex->data, buf, hdr.datadesc[0].size);
		}
		glCompressedTexImage2D(GL_TEXTURE_2D, i, hdr.glfmt, hdr.width, hdr.height, 0,
				hdr.datadesc[i].size, buf);

		hdr.width /= 2;
		hdr.height /= 2;
	}

	free(buf);
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
	case GL_LUMINANCE:
	case 1:
		return "GL_LUMINANCE";
	case GL_RGB:
	case 3:
		return "GL_RGB";
	case GL_RGBA:
	case 4:
		return "GL_RGBA";
	case GL_BGR: return "GL_BGR";
	case GL_BGRA: return "GL_BGRA";
	case GL_SLUMINANCE: return "GL_SLUMINANCE";
	case GL_SLUMINANCE8: return "GL_SLUMINANCE8";
	case GL_SLUMINANCE_ALPHA: return "GL_SLUMINANCE_ALPHA";
	case GL_SLUMINANCE8_ALPHA8: return "GL_SLUMINANCE8_ALPHA8";
	case GL_SRGB: return "GL_SRGB";
	case GL_SRGB8: return "GL_SRGB8";
	case GL_SRGB_ALPHA: return "GL_SRGB_ALPHA";
	case GL_SRGB8_ALPHA8: return "GL_SRGB8_ALPHA8";
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
