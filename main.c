#include <stdio.h>
#include "se.h"
#include "seImage.h"
#include "seFont.h"
#include "math/Math.h"
#include <string.h>
#include <stdlib.h>

#include "seShaderMan.h"

static int app_state;// 0=slash,1=run,2=pause,3=menu
static int game_level;// 1,2,3, pause menu

static GLuint prog;
static GLuint prog2;
static GLuint progText;

static GLint prog_proj_loc;
static GLint prog2_proj_loc;
static GLint prog2_smooth_loc;
static GLint progText_proj_loc;

mat4 proj;

GLuint ftex;
float font_hieght, font_size = 32.f;

typedef struct {
	float l, r, t, b;
} Quad;

int inreck(int x, int y, Quad* q) {
	if (x < q->l || x > q->r) return 0;
	if (y < q->t || y > q->b) return 0;
	return 1;
}

Quad quads[16];
float anim = 0.f;
int id_anim = -1;

char* id_names[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "0" };
char field[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
int id_zero;
char moves[80];
int moves_count;

vec2 centers_field[16];
float quad_width;

GLuint g_ibuffer;
Vtx2tc text_vtx[235];//[40];//10 chars
int text_count;

static seText* text_ids;

static seFont _font;
static float spacex, spacey;

static GLuint quads_vb;
static int quads_off[15] = {
	0,
	160,
	320,
	480,
	640,
	800,
	960,
	1120,
	1280,
	1440,
	1680,
	1920,
	2160,
	2400,
	2640
};

char can_move[16][4] = {
	{4, 1, -1, -1},
	{5, 0, 2, -1},
	{6, 1, 3, -1},
	{7, 2, -1, -1},
	{0, 8, 5, -1},
	{1, 9, 4, 6},
	{2, 10, 5, 7},
	{3, 11, 6, -1},
	{4, 12, 9, -1},
	{5, 13, 8, 10},
	{6, 14, 9, 11},
	{7, 15, 10, -1},
	{8, 13, -1, -1},
	{9, 12, 14, -1},
	{10, 13, 15, -1},
	{11, 14, -1, -1}
};

void print_vtx(Vtx2tc* v, int c) {
	int i;
	print("vtx:\n");
	for (i = 0; i < c; ++i) {
		//vertText* v = &vtx[i];
		print("\t%.2f %.2f  %.2f %.2f %u\n", v[i].pos.x, v[i].pos.y, v[i].uv.x, v[i].uv.y, v[i].col);
	}
}

void init_ibuffer(/*int count*/) { //TODO: set max size
	int i;
	uint16_t* iGen;
	uint16_t* iptr;
	// quads count 45 // max 65536
	int iGen_count = 268;//268;// 45*6-2 TODO: set max size
	iGen = (uint16_t*)malloc(2 * (iGen_count + 2));//for 2 last indices
	iptr = iGen;
	for (i = 0; i < 180; ++i) {//45*4 vertex count
		*(uint16_t*)iptr = i;
		++iptr;
		if ((i % 4) - 3 == 0) {
			*iptr = i;
			++iptr;
			*iptr = i + 1;
			++iptr;
		}
	}
	glGenBuffers(1, &g_ibuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, iGen_count * 2, iGen, GL_STATIC_DRAW);
	free(iGen);
}


//vertex count 144 = 15(quads)+9(one number)+6(12)(two numder) * 4
void init_quads_vb() {
	int i;
	float half_width = quad_width * 0.5f;
	float it = 0.99f;//250 / 256.f;
	Vtx2tc* vtx = (Vtx2tc*)malloc(144 * 20);
	Vtx2tc* vtxptr = vtx;

	for (i = 0; i < 16; ++i) {
		float l, r, t, b;
		int x = field[i];
		seText* tt = &text_ids[x];
		if (x == 15) continue;
		vtxptr = vtx + quads_off[x] / 20;
		l = centers_field[i].x - half_width;
		r = centers_field[i].x + half_width;
		t = centers_field[i].y - half_width;
		b = centers_field[i].y + half_width;

		VTXc(vtxptr[0], l, t, spacex, spacey, 0xffdddddd);
		VTXc(vtxptr[1], r, t, spacex, spacey, 0xffdddddd);
		VTXc(vtxptr[2], l, b, spacex, spacey, 0xffdddddd);
		VTXc(vtxptr[3], r, b, spacex, spacey, 0xffdddddd);

		if (tt->font_size != half_width * 0.5f) Text_size_d(tt, half_width * 0.5f);

		l = centers_field[i].x - tt->area[2] * 0.5f;
		t = centers_field[i].y - tt->area[3] * 0.5f;
		Text_getvtx(tt, l, t, &vtxptr[4]);
	}

	glGenBuffers(1, &quads_vb);
	glBindBuffer(GL_ARRAY_BUFFER, quads_vb);
	glBufferData(GL_ARRAY_BUFFER, 144 * 20, vtx, GL_STATIC_DRAW);

	free(vtx);
}

void move_quad_vb(int m, float f) {
	float half_width = quad_width * 0.5f;
	float l, r, t, b, x, y;
	Vtx2tc vtx[12];
	int i = field[m], n;
	seText* tt = &text_ids[i];

	x = lerp(centers_field[m].x, centers_field[id_zero].x, f);
	y = lerp(centers_field[m].y, centers_field[id_zero].y, f);

	l = x - half_width;
	r = x + half_width;
	t = y - half_width;
	b = y + half_width;

	VTXc(vtx[0], l, t, spacex, spacey, 0xffdddddd);
	VTXc(vtx[1], r, t, spacex, spacey, 0xffdddddd);
	VTXc(vtx[2], l, b, spacex, spacey, 0xffdddddd);
	VTXc(vtx[3], r, b, spacex, spacey, 0xffdddddd);

	n = 4;
	l = x - tt->area[2] * 0.5f;
	t = y - tt->area[3] * 0.5f;
	n += Text_getvtx(tt, l, t, &vtx[n]) * 4;

	glBindBuffer(GL_ARRAY_BUFFER, quads_vb);
	glBufferSubData(GL_ARRAY_BUFFER, quads_off[i], n * 20, vtx);
}

void init_centers() {
	float w;
	float woff = 0, hoff = 0;
	float half_width;
	int i;
	if (App.w < App.h) {
		w = App.w * 0.25f;
		hoff = App.h / 2.f - w * 2.f;
	}
	else {
		w = App.h * 0.25f;
		woff = App.w / 2.f - w * 2.f;
	}
	quad_width = w - 10.f;
	half_width = quad_width * 0.5f;
	for (i = 0; i < 16; ++i) {
		float l, t;
		l = (i % 4) * w + woff + 5;
		t = (i / 4) * w + hoff + 5;
		centers_field[i].x = l + half_width;
		centers_field[i].y = t + half_width;

		quads[i].l = l;
		quads[i].r = l + quad_width;
		quads[i].t = t;
		quads[i].b = t + quad_width;
	}
}

int checkState() {
	int i, j;
	int sgn = 0;
	for (i = 0; i < 15; ++i) {
		for (j = i + 1; j < 15; ++j) {
			if (field[i] > field[j]) {
				++sgn;
			}
		}
	}

	return sgn % 2;
}

void Resize(int w, int h) {
	int i;
	createOrthographicOffCenter(0, (float)w, (float)h, 0, 0, 1, &proj);
	glUseProgram(prog);
	glUniformMatrix4fv(prog_proj_loc, 1, GL_FALSE, proj.m);

	glUseProgram(prog2);
	glUniformMatrix4fv(prog_proj_loc, 1, GL_FALSE, proj.m);

	glUseProgram(progText);
	glUniformMatrix4fv(progText_proj_loc, 1, GL_FALSE, proj.m);

	init_centers();
	init_quads_vb();
	i = checkState();

}

void shuffle(int n) {
	int i, id, lid = -1;
	for (i = 0; i < n; ++i) {
		do {
			int r = rand() % 4;
			id = can_move[id_zero][r];
		} while (id == -1 || lid == id);
		field[id_zero] = field[id];
		field[id] = 15;
		lid = id_zero;
		id_zero = id;
	}
}

int getH() {
	int i, h = 0;
	for (i = 0; i < 16; ++i) {
		int x = field[i];
		int xx = x % 4;
		int yx = x / 4;
		int xi = i % 4;
		int yi = i / 4;
		int px, py;
		if (x == 15) continue;
		px = abs(xx - xi);
		py = abs(yx - yi);
		h += px + py;
		//print("%d %d\n", i, px + py);
	}
	print("h: %d\n", h);
	return h;
}

void resolve() { // FIXME
	int i, id, h, li = 1000, lh = 500;
	int m[4];
	h = getH();
	while (h != 0) {
		lh = 500;
		for (i = 0; i < 4; ++i) {
			id = can_move[id_zero][i];
			if (id == -1)break;
			field[id_zero] = field[id];
			field[id] = 15;
			id_zero = id;
			m[i] = id;
			h = getH();
			if (h < lh) { li = i; lh = h; }
		}
		moves[moves_count++] = m[li];
		//h = getH();
	}
}

static void print_ext() {
	const GLubyte* extensions = glGetString(GL_EXTENSIONS);
	char loc[64];
	int i;
	FILE* f;

	if (extensions == NULL) {
		return;
	}
	f = fopen("ext.txt", "w");

	//if((*extensions == ' ' || *extensions == '\0')) return ;
	while (*extensions) {
		for (i = 0; *extensions != ' '; ++i) {
			loc[i] = *extensions++;
		}

		++extensions;
		loc[i] = '\n';
		fwrite(loc, 1, i + 1, f);
		loc[i] = '\0';
		print("%s\n", loc);
	}

	fclose(f);

	return;
}

//static seText _text;
static seText _text2;

typedef struct {
	float start, dst;
	float* src;
	float t;
	float mt; //multiply time
	int state;
	//int type;
	int dtype;// data type, int, int4, float..??
}Curve;

int Curve_update(Curve* inter, float dt);
void Curve_set(Curve* inter, float dst);
void Curve_set_mt(Curve* inter, float dst, float mt);

void Border_make(Vtx2tc* vtx, float b, Quad* rect, Quad* uv) {
	float x0 = rect->l - b, x1 = rect->l, x2 = rect->r, x3 = rect->r + b;
	float y0 = rect->t - b, y1 = rect->t, y2 = rect->b, y3 = rect->b + b;
	float s0 = uv->l, s1 = uv->r, t0 = uv->t, t1 = uv->b;

	// left top
	VTX(vtx[0], x0, y0, s1, t1);
	VTX(vtx[1], x1, y0, s0, t1);
	VTX(vtx[2], x0, y1, s1, t0);
	VTX(vtx[3], x1, y1, s0, t0);

	// top
	VTX(vtx[4], x1, y0, s0, t1);
	VTX(vtx[5], x2, y0, s0, t1);
	VTX(vtx[6], x1, y1, s0, t0);
	VTX(vtx[7], x2, y1, s0, t0);

	// right top
	VTX(vtx[8], x2, y0, s0, t1);
	VTX(vtx[9], x3, y0, s1, t1);
	VTX(vtx[10], x2, y1, s0, t0);
	VTX(vtx[11], x3, y1, s1, t0);

	// left 
	VTX(vtx[12], x0, y1, s1, t0);
	VTX(vtx[13], x1, y1, s0, t0);
	VTX(vtx[14], x0, y2, s1, t0);
	VTX(vtx[15], x1, y2, s0, t0);

	// center
	/*VTX(vtx[12], x1, y1, s0, t0);
	VTX(vtx[13], x2, y1, s0, t0);
	VTX(vtx[14], x1, y2, s0, t0);
	VTX(vtx[15], x2, y2, s0, t0);*/

	// right
	VTX(vtx[16], x2, y1, s0, t0);
	VTX(vtx[17], x3, y1, s1, t0);
	VTX(vtx[18], x2, y2, s0, t0);
	VTX(vtx[19], x3, y2, s1, t0);

	// left bottom
	VTX(vtx[20], x0, y2, s1, t0);
	VTX(vtx[21], x1, y2, s0, t0);
	VTX(vtx[22], x0, y3, s1, t1);
	VTX(vtx[23], x1, y3, s0, t1);

	// bottom
	VTX(vtx[24], x1, y2, s0, t0);
	VTX(vtx[25], x2, y2, s0, t0);
	VTX(vtx[26], x1, y3, s0, t1);
	VTX(vtx[27], x2, y3, s0, t1);

	// right bottom
	VTX(vtx[28], x2, y2, s0, t0);
	VTX(vtx[29], x3, y2, s1, t0);
	VTX(vtx[30], x2, y3, s0, t1);
	VTX(vtx[31], x3, y3, s1, t1);
}


void Curve_init(Curve* inter, float* src) {
	inter->state = 0;
	inter->src = src;
	inter->t = 0;
	inter->mt = 0.049f;
}

void Curve_set(Curve* inter, float dst) {
	inter->state = 1;
	inter->dst = dst;
	inter->start = *inter->src;
	inter->t = 0;
	inter->mt = 0.049f;
}

void Curve_set_mt(Curve* inter, float dst, float mt) {
	inter->state = 1;
	inter->dst = dst;
	inter->start = *inter->src;
	inter->t = 0;
	inter->mt = mt;
}

int Curve_update(Curve* inter, float dt) {
	if (!inter->state) return 1;
	inter->t += dt * inter->mt;//*0.049f;
	if (inter->t >= 1.f || fabsf(*inter->src - inter->dst) < 0.01f) {
		inter->t = 0;
		inter->state = 0;
		*inter->src = inter->dst;
		return 1;
	}
	else {
		//float t = getT(inter->type,inter->t);
		*inter->src = lerp(inter->start, inter->dst, inter->t);
	}

	return 0;
}



static const char se2d_vert[] = TOSTR(
	attribute vec4 a_pos;
attribute vec4 a_col;

uniform mat4 u_proj;

varying vec4 v_col;
varying vec2 v_tex;

void main() {
	gl_Position = u_proj * vec4(a_pos.xy, 0, 1);
	v_tex = a_pos.zw;
	v_col = a_col;
});

static const char seTextColDF_frag[] = TOSTR(
	varying vec4 v_col;
varying vec2 v_tex;

uniform sampler2D u_tex;
uniform float u_smooth;

void main() {
	float dist = texture2D(u_tex, v_tex).a;
	float alpha = smoothstep(0.5 - u_smooth, 0.5 + u_smooth, dist);
	gl_FragColor = v_col;
	gl_FragColor.a = alpha * v_col.a;
});

void Init() {
	int i;
	float tw;
	
	Font_init(&_font, "res/qarmic_p.fnt");

	spacex = (_font.glyphs[0].x) * _font.tws;
	spacey = (_font.glyphs[0].y) * _font.ths;

	text_ids = (seText*)malloc(sizeof(seText) * 15);
	for (i = 0; i < 15; ++i) {
		Text_init_d(&text_ids[i], id_names[i], 16.f, 0xffff1100, &_font);
	}

	Text_init_ex(&_text2, "hi, pazzle!?", &_font, 48.f, 1);
	tw = Text_width("hi, pazzle!?", 48.f, &_font);
	Text_make(&_text2, (App.w - tw) * 0.5f, 90);
	Text_set_color(&_text2, 1.f, 0, 0, 1.f);
	//print_ext();

	prog = getProg(S2DADiscard);
	progText = getProg(S2DText);

	prog2 = glCreateProgram();
	glAttachShader(prog2, createShader(GL_VERTEX_SHADER, se2d_vert));
	glAttachShader(prog2, createShader(GL_FRAGMENT_SHADER, seTextColDF_frag));
	glLinkProgram(prog2);

	deleteShaders();
	prog_proj_loc = glGetUniformLocation(prog, "u_proj");
	prog2_proj_loc = glGetUniformLocation(prog2, "u_proj");
	prog2_smooth_loc = glGetUniformLocation(prog2, "u_smooth");
	progText_proj_loc = glGetUniformLocation(progText, "u_proj");


	glClearColor(0.0, 0.5, 1, 1);

	init_ibuffer();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	for (i = 0; i < 16; ++i) {
		if (field[i] == 15) id_zero = i;
	}

	shuffle(10);
}

float _time;
int prev_z = -1;
void Update(float f) {
	static int lsec = -1;
	static int msec = 0;

	if (app_state == 0) {
		static float time = 0;
		static float alpha = 0;
		time += 0.000049f * f;
		if (alpha < 1.f) {
			alpha += 0.00049f * f;
			if (alpha >= 1.f)alpha = 1.f;
		}
		Text_set_color(&_text2, alpha, alpha, alpha, alpha);
		if (time > 1.f)++app_state;
		return;
	}

	_time += f;

	msec = ((int)_time / 10) % 100;
	if (lsec != msec) {
		int rsec = ((int)_time / 1000) % 60;
		int min = ((int)_time / 1000 / 60) % 60;
		char _text[512];

		//sprintf(_text, "%.2d:%.2d:%.2d", min, rsec, msec);
		sprintf(_text, "%d", App.fps);

		text_count = Text_build(text_vtx, _text, 4, 0, 0xffffffff, &_font) * 6 - 2;

		lsec = msec;
	}

	if (id_anim != -1) {
		anim += f * 0.0049f;

		if (anim >= 1.f) {
			move_quad_vb(id_anim, 1.f);
			//zero swap
			field[id_zero] = field[id_anim];
			field[id_anim] = 15;
			anim = 0;// timer
#if 1
			id_zero = id_anim;
			id_anim = -1; // stop anim
			//getH();
#else
			prev_z = id_zero;// for shuffle
			id_zero = id_anim;
			do {
				int r = rand() % 4;
				id_anim = can_move[id_zero][r];
			} while (id_anim == -1 || prev_z == id_anim);
#endif
		}
		else {
			move_quad_vb(id_anim, anim);
		}
	}
}

float aver, avert;
void Render(float f) {
	double start;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibuffer);
	glEnableVertexAttribArray(0);

	if (app_state == 0) {
		Text_draw(&_text2);
		return;
	}

	glEnableVertexAttribArray(1);


	start = seTime();

	glBindTexture(GL_TEXTURE_2D, _font.tex);
	glUseProgram(prog2);

	{float smooth = 0.25f / (quad_width * 0.25f * _font.inv_size);
	glUniform1f(prog2_smooth_loc, smooth); }
	// quads
	glBindBuffer(GL_ARRAY_BUFFER, quads_vb);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 20, 0);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 20, (void*)16);
	glDrawElements(GL_TRIANGLE_STRIP, 268, GL_UNSIGNED_SHORT, 0);//45*6-2

	{float smooth = 0.25f / (16.f * _font.inv_size);
	glUniform1f(prog2_smooth_loc, smooth); }
	// time
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 20, text_vtx);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 20, &text_vtx->col);
	glDrawElements(GL_TRIANGLE_STRIP, text_count, GL_UNSIGNED_SHORT, 0);

	Text_draw_end();

	aver = (float)(seTime() - start);
	avert += f / 1000.f;
	if (avert >= 1.f) {
		//print("bind tex: %fms %dfps\n", aver, App.fps);
		aver = 0;
		avert = 0;
	}
}

void Exit() {
	Font_free(&_font);
	glDeleteBuffers(1, &g_ibuffer);
	glDeleteBuffers(1, &quads_vb);
	glDeleteTextures(1, &ftex);
	deleteProgs();
}

void Click(int e, int x, int y) {
	if (app_state == 0) {
		++app_state;
		return;
	}
	if (e == SE_PRESS) {
		int i;

		if (id_anim != -1) return;
		for (i = 0; i < 4; ++i) {
			int move_to = can_move[id_zero][i];
			if (move_to == -1)break;
			if (inreck(x, y, &quads[move_to])) {
				id_anim = move_to;
				break;
			}
		}
	}
}

int main(int argc, char** argv) {
	(void)argc; (void)argv;
	App.init = Init;
	App.resize = Resize;
	App.update = Update;
	App.render = Render;
	App.exit = Exit;
	App.mevent = Click;

	seInit();
	seRun();

	return 0;
}
