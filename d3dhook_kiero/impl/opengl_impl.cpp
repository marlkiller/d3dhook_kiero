#include "../imgui_constants.h"
#include "opengl_impl.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../logger.h"
#include "../detours.h"

#include <exception>
#include "../imgui/imgui_impl_opengl2.h"
//#include "../imgui/imgui_impl_opengl3.h"
#include <string>
#include "../imgui/soil.h"
#include "../imgui/GL.h"
#include "../imgui/glcorearb.h"
#include "../common_utils.h"
#include "../imgui_draw_util.h"



typedef BOOL(APIENTRY* wglSwapBuffers)(HDC  hdc);
typedef void (APIENTRY* glBegin_t)(GLenum mode);
typedef void (APIENTRY* glClear_t)(GLbitfield mask);

typedef void (APIENTRY* glColor4f_t)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
//typedef void (APIENTRY* glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
//typedef void (APIENTRY* glVertex3f)(GLfloat x, GLfloat y, GLfloat z);


wglSwapBuffers oWGLSwapBuffers;
glBegin_t oGLBegin;
glClear_t oGLClear;
glColor4f_t oGLColor4f;

static bool coloring = false;

GLuint texture_id[1024];



void LoadTextureFile(int index, const char* image)
{
	int width, height;
	GLint last_texture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGenTextures(1, &texture_id[index]);
	glBindTexture(GL_TEXTURE_2D, texture_id[index]);
	//unsigned char* soilimage = SOIL_load_image_from_memory(image, size, &width, &height, 0, SOIL_LOAD_RGBA);
	unsigned char* soilimage = SOIL_load_image(image, &width, &height, 0, SOIL_LOAD_RGBA);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // GL_REPEAT���������ȱ���Сʱ�ظ�ʹ������������ÿ���㡣GL_CLAMP����1��ĵ���1����0С�ĵ���0��bilinear�����β�ֵ�����ȸ��ߣ�����Ҫ�Լ����ּ���
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // GL_NEAREST��ȡ�ȽϽӽ����Ǹ����ء�GL_LINEAR������Χ�ĸ����ص�ƽ��ֵ��Ϊ������bilinear�����β�ֵ�����ȸ��ߣ�����Ҫ�Լ����ּ���
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, soilimage); // ʹ��glTexImage2D()ʱ�����õ�λͼ�ļ��ֱ��ʱ���Ϊ��64��64��128��128��256��256���ָ�ʽ�����������С�����ֻ��Ʋ�������
	SOIL_free_image_data(soilimage);
	glBindTexture(GL_TEXTURE_2D, last_texture);
}

void LoadTextureMemary(int index, int resource_id, char* resource_type)
{
	int width, height;
	GLint last_texture;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	glGenTextures(1, &texture_id[index]);
	glBindTexture(GL_TEXTURE_2D, texture_id[index]);



	HINSTANCE hInst = GetModuleHandle("d3dhook_kiero.dll");
	HRSRC res = ::FindResource(hInst, MAKEINTRESOURCE(resource_id), TEXT(resource_type));
	DWORD size = ::SizeofResource(hInst, res);
	HGLOBAL mem = ::LoadResource(hInst, res);
	LPVOID raw_data = ::LockResource(mem);
	OUTPUT_DEBUG(L"read_resource > res {%d}, size {%d} , mem {%d}", res, size, mem);
	unsigned char* soilimage = SOIL_load_image_from_memory((unsigned char*)(raw_data), size, &width, &height, 0, SOIL_LOAD_RGBA);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, soilimage);
	SOIL_free_image_data(soilimage);
	glBindTexture(GL_TEXTURE_2D, last_texture);
}

void generate_texture(int index,int width, int height) {

	
}
	

void APIENTRY hkGLColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{

	red = 1 * red, green = 1 * green, blue = 1 * blue;
	(*oGLColor4f)(red, green, blue, alpha);
}


static float depth_range_zNear = 1;
static float depth_range_zFar = 1;
static bool enable_depth_dev = false;
static bool enable_draw_esp = false;
static bool open_dev = false;
HANDLE g_hProcess;
DWORD cstrike_base;

const DWORD el_num_base = 0x620FCC; //cstrike.exe + 620FCC          //��Ϸ�г����Լ�����������
 const DWORD Control_CursorAngle_X_offset = 0x19E10C8;			//���x�Ƕ�
const DWORD Control_CursorAngle_Y_offset = 0x19E10C4;			//���y�Ƕ�

typedef struct PlayerData
{
	float position[3];
	float hp;
}PlayerData;

RECT  g_winRect = { 0 };

bool WorldToScreen(float position[3], float screen[2], float matrix[16], int windowWidth, int windowHeight)
{
    //Matrix-vector Product, multiplying world(eye) coordinates by projection matrix = clipCoords
	float clipCoords[4];
	clipCoords[0] = position[0] * matrix[0] + position[1] * matrix[4] + position[2] * matrix[8] + matrix[12];
	clipCoords[1] = position[0] * matrix[1] + position[1] * matrix[5] + position[2] * matrix[9] + matrix[13];
	clipCoords[2] = position[0] * matrix[2] + position[1] * matrix[6] + position[2] * matrix[10] + matrix[14];
	clipCoords[3] = position[0] * matrix[3] + position[1] * matrix[7] + position[2] * matrix[11] + matrix[15];

	if (clipCoords[3] < 0.1f)
		return false;

	//perspective division, dividing by clip.W = Normalized Device Coordinates
	float NDC[3];
	NDC[0] = clipCoords[0] / clipCoords[3];
	NDC[1] = clipCoords[1] / clipCoords[3];
	NDC[2] = clipCoords[2] / clipCoords[3];

	screen[0] = (windowWidth / 2 * NDC[0]) + (NDC[0] + windowWidth / 2);
	screen[1] = -(windowHeight / 2 * NDC[1]) + (NDC[1] + windowHeight / 2);
    return true;
}
void LoadGameInfo() {

	g_hProcess = GetCurrentProcess();
	cstrike_base = (uintptr_t)GetModuleHandle("cstrike.exe");
	LOG_INFO("g_hProcess -> %x, cstrike.exe -> %x", g_hProcess, cstrike_base);

}
void ReadDataList(int index, PlayerData* data)
{
	//[[[cstrike.exe + 11069BC]+ 7C + (i*324)] + 4] + 8
	float dev = 0;
	DWORD addr;
	ReadProcessMemory(g_hProcess, (PBYTE*)(cstrike_base + 0x11069BC), &addr, sizeof(DWORD), 0);
	ReadProcessMemory(g_hProcess, (PBYTE*)(addr + 0x7C + (index * 0x324)), &addr, sizeof(DWORD), 0);//�����˵Ļ�ַ
	ReadProcessMemory(g_hProcess, (PBYTE*)(addr + 0x4), &addr, sizeof(DWORD), 0);//������2����ַ
	ReadProcessMemory(g_hProcess, (PBYTE*)(addr + 0x8), &data->position, sizeof(float[3]), 0);//������λ��
	ReadProcessMemory(g_hProcess, (PBYTE*)(addr + 0x160), &data->hp, sizeof(float), 0);//������Ѫ��



}
#define PI 3.1415926



float GetDistance3D(float MyPos[3], float ObjPos[3])
{
	return sqrt
	(
		pow((double)(ObjPos[0] - MyPos[0]), 2.0) +
		pow((double)(ObjPos[1] - MyPos[1]), 2.0) +
		pow((double)(ObjPos[2] - MyPos[2]), 2.0)
	);
}


void DrawOpenGLDIY() {


	if (p_open)
	{
		ImGui::Begin("OpenGLDraw");
		ImGui::Checkbox("DrawESP", &enable_draw_esp);
		ImGui::End();

	}

	if (!enable_draw_esp)
	{
		return;
	}
	PlayerData my_data = { 0 };
	ReadDataList(0, &my_data);
	
	if (my_data.hp <= 1 || my_data.hp > 100) // FIXME Alive
	{
		return;
	}

	OUTPUT_DEBUG(L"my_data.hp > %f", my_data.hp);

	int numb = 0;
	memcpy(&numb, (PBYTE*)(cstrike_base + el_num_base), sizeof(numb));

	float CursorAngle_X;
	float CursorAngle_Y;
	ReadProcessMemory(g_hProcess, (PBYTE*)(cstrike_base + Control_CursorAngle_X_offset), &CursorAngle_X, sizeof(CursorAngle_X), 0);
	ReadProcessMemory(g_hProcess, (PBYTE*)(cstrike_base + Control_CursorAngle_Y_offset), &CursorAngle_Y, sizeof(CursorAngle_Y), 0);

	for (int i = 1; i <= numb; i++)
	{
		PlayerData el_data = { 0 };
		ReadDataList(i, &el_data);
		

		float diff_x, diff_y, diff_z,diff_mouse;
		diff_x = my_data.position[0] - el_data.position[0];
		diff_y = my_data.position[1] - el_data.position[1];
		diff_z = my_data.position[2] - el_data.position[2];
		float angle_x = 0;
		if (diff_x<0 && diff_y<0)
			angle_x = atan(abs(diff_y / diff_x)) / PI * 180;
		else if (diff_x < 0 && diff_y==0)
			angle_x =0;
		else if (diff_x == 0 && diff_y < 0)
			angle_x = 90;
		else if (diff_x > 0 && diff_y < 0)
			angle_x = 90 + atan(abs(diff_x/diff_y))/PI*180;
		else if (diff_x > 0 && diff_y == 0)
			angle_x = 180;
		else if (diff_x > 0 && diff_y > 0)
			angle_x = 180 + atan(abs(diff_y/diff_x))/PI*180;
		else if (diff_x == 0 && diff_y > 0)
			angle_x = 270;
		else if (diff_x < 0 && diff_y > 0)
			angle_x = 270 + atan(abs(diff_x	/diff_y))/PI* 180;

		float instance_2d, instance_3d = -1;
		instance_2d = sqrt(pow(diff_x, 2) + pow(diff_y, 2));
		instance_3d = sqrt(pow(diff_x, 2) + pow(diff_y, 2) + pow(diff_z, 2));

		diff_mouse = CursorAngle_X - angle_x;
		if (angle_x- CursorAngle_X > 180)
			diff_mouse = 360 - angle_x + CursorAngle_X;
		else if (CursorAngle_X - angle_x>180) 
			diff_mouse = (360 - CursorAngle_X + angle_x)*-1;

		// TODO 
 		float size_x = 1024;
		float size_y = 768;

		float fov = 90;
		float tmp,tmp2,screen_x,screen_y,w,h;
		float compare = 1000 / instance_3d;
		tmp = tan(diff_mouse*PI/180);
		screen_x = size_x / 2 + (tmp)*size_x / 2 - 8 * compare * 90 / fov;
		// AngleMouse ���ǶȼӸ����ţ�Ҫȡ�෴ֵ
		// ���Ӹ��Ż���ɷ�����̧��ĽǶ�����
		tmp2 = tan(-CursorAngle_Y * PI / 180) * instance_2d + diff_z;
		screen_y = size_y / 2 + tmp2 / instance_2d * 512 * 90 / fov - 5;

		w = 16.666666 * compare * 90 / fov;
		h = 33.666666 * compare * 90 / fov;
		//OUTPUT_DEBUG(L"screen (%f,%f,%f,%f) , instance_3d {%f}", screen_x, screen_y,w,h, instance_3d);
		//DrawBox(screen_x, screen_y, w, h, ImColor(color_pick));
		DrawEspBox(box_type, screen_x, screen_y, w, h, 255, 255, 255, 255);



	}
	
}
BOOL __stdcall hkWGLSwapBuffers(HDC hdc)
{

	try
	{
		if (!init) 
		{
			auto tStatus = true;

			LOG_INFO("imgui first init {%d}", init);
			GAME_HWND = WindowFromDC(hdc);
			oWndProcHandler = (WNDPROC)SetWindowLongPtr(GAME_HWND, WNDPROC_INDEX, (LONG_PTR)hWndProc);

			/*gglGetIntegerv(GL_MAJOR_VERSION, &iMajor);
			lGetIntegerv(GL_MINOR_VERSION, &iMinor);*/

			/*if ((iMajor * 10 + iMinor) >= 32)
				bOldOpenGL = false;*/
			//LOG_INFO("Is bOldOpenGL -> {%d}", bOldOpenGL);

			// TODO Support OpenGL3
			bool bOldOpenGL = false;

			ImGui::CreateContext();
			ImGui_ImplWin32_Init(GAME_HWND);
		  

			ImGui_ImplOpenGL2_Init();
			//LoadTextureFile(1,"C:\\Users\\voidm\\Desktop\\111.jpg"); // TODO move to resource
			LoadTextureMemary(1,IDB_PNG1,"PNG"); // RED
			LoadTextureMemary(2,IDB_PNG2,"PNG"); // GREEN
			LoadTextureMemary(3, IDB_BITMAP1,"Bitmap"); // DIY

			LoadGameInfo();
 			init = true;
		}


		ImGui_ImplOpenGL2_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		DrawMainWin();
		DrawOpenGLDIY();

		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());



	}
	catch (...) {
		std::exception_ptr p = std::current_exception();
		LOG_ERROR("ERROR");
		LOG_ERROR("ERROR : {%s}", p);
		throw;
	}

	return oWGLSwapBuffers(hdc);
}



void APIENTRY hkGLBegin(GLenum mode)
{
	//OUTPUT_DEBUG(L"hkGLBegin >> {%d}", mode);

	bool matched = false;
	// change the mode
	if ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(0x31) & 1))
	{
		radio_stride++;
	}
	if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(0x31) & 1))
	{
		radio_stride--;
	}
	// change the current_item
	if ((GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState(0x30) & 1))
	{
		if (current_count + 1 < table_items.size())
		{

			current_count = current_count + 1;
			current_item = table_items[current_count];
			OUTPUT_DEBUG(L"current_count %d/%d --> ( %d )", table_items.size(), current_count + 1, current_item.Stride);
		}
	}
	if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(0x30) & 1))
	{
		if (current_count > 0)
		{
			current_count = current_count - 1;
			current_item = table_items[current_count];
			OUTPUT_DEBUG(L"current_count %d/%d --> ( %d )", table_items.size(), current_count + 1, current_item.Stride);
		}
	}

	if (refresh_draw_items) 
	{
		bool exist = false;
		ImVector<DrawItem>::iterator it;
		for (it = table_items.begin(); it != table_items.end(); it++)
			if (it->Stride == mode)
			{
				exist = true;
				continue;
			}
		if (!exist) {
			id_number = id_number + 1;
			DrawItem item;
			item.ID = id_number;
			item.Stride = mode;
			table_items.push_back(item);
		}
	}
	if (find_model_type == 1 && current_count >= 0)
	{
		if (current_item.Stride == mode)
		{
			if ((GetAsyncKeyState(VK_END) & 1))
			{
				LOG_INFO("Table:Target obj is=\t(mode==%d)", mode);
			}
			matched = true;
		}
	}

	if (radio_stride == mode && find_model_type == 2){
		matched = true;
	}
	if (matched && (wall_hack_type > 0 || draw_cclor_type > 0))
	{

		if (enable_depth_dev)
		{
			glDepthRange(depth_range_zNear, depth_range_zFar);
		}

		if (wall_hack_type ==1)
		{
			// https://blog.csdn.net/qq_43872529/article/details/102496602
			glDisable(GL_DEPTH_TEST);
			/*if (Player || Weapon)
				glDepthRange(0, 0.5);
			else
				glDepthRange(0.5, 1);*/
		}
		
		if (draw_cclor_type != 2)
		{
			//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // close
			// GL_LINE , Must be work with hkGLClear open
			/*glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			glLineWidth(3.0);
			glColor3f(255, 255, 255);*/
		}
		
		if (draw_cclor_type == 1)
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			glBindTexture(GL_TEXTURE_2D, texture_id[1]);
		}else if (draw_cclor_type ==2)
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			glBindTexture(GL_TEXTURE_2D, texture_id[2]);
			/*glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			glBindTexture(GL_TEXTURE_2D, texture_id[2]);*/


			//���Ҫʹ��͸����, ����Ҫ���û��:glEnable(GL_BLEND);
			// RE DRAW
			/*const GLfloat squareVertices[] = {
			0.5, 0.5, 0.0,
			-0.5, 0.5, 0.0,
			0.5, -0.5, 0.0,
			-0.5, -0.5, 0.0 };
			glEnableClientState(GL_VERTEX_ARRAY);
			glColor4f(1.0, 0.0, 0.0, 0.5);
			glLoadIdentity();
			glTranslatef(0, 0, -5);
			glVertexPointer(3, GL_FLOAT, 0, squareVertices);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);*/

			// FIXME 
			/*glEnable(GL_TEXTURE_2D);
			glColor4f(1.0, 0.0, 0.0, 0.5);
			glDisable(GL_TEXTURE_2D);*/
		}
		
	}

	return oGLBegin(mode);
}

void APIENTRY hkGLClear(GLbitfield mask)
{
	if (mask == GL_DEPTH_BUFFER_BIT)
	{
		//mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		//(*oGLClear)(GL_COLOR_BUFFER_BIT), glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	}

	return oGLClear(mask);

}

void impl::opengl::init()
{
	HMODULE hPENGLDLL = 0;
	do
	{
		hPENGLDLL = GetModuleHandle("opengl32.dll");
		Sleep(1000);
		LOG_INFO("GetModuleHandle with opengl32.dll..{%x}", hPENGLDLL);
	} while (!hPENGLDLL);
	Sleep(100);


	oWGLSwapBuffers = (wglSwapBuffers)GetProcAddress(hPENGLDLL, "wglSwapBuffers");
	oGLBegin = (glBegin_t)GetProcAddress(hPENGLDLL, "glBegin");
	oGLClear = (glClear_t)GetProcAddress(hPENGLDLL, "glClear");
	oGLColor4f = (glColor4f_t)GetProcAddress(hPENGLDLL, "glColor4f");


	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(LPVOID&)oWGLSwapBuffers, (PBYTE)hkWGLSwapBuffers);
	DetourAttach(&(LPVOID&)oGLBegin, (PBYTE)hkGLBegin);
	DetourAttach(&(LPVOID&)oGLClear, (PBYTE)hkGLClear);
	DetourAttach(&(LPVOID&)oGLColor4f, (PBYTE)hkGLColor4f);
	DetourTransactionCommit();

}