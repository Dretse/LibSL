/* --------------------------------------------------------------------
Author: Sylvain Lefebvre    sylvain.lefebvre@sophia.inria.fr

                  Simple Library for Graphics (LibSL)

This software is a computer program whose purpose is to offer a set of
tools to simplify programming real-time computer graphics applications
under OpenGL and DirectX.

This software is governed by the CeCILL-C license under French law and
abiding by the rules of distribution of free software.  You can  use, 
modify and/ or redistribute the software under the terms of the CeCILL-C
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info". 

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability. 

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or 
data to be ensured and,  more generally, to use and operate it in the 
same conditions as regards security. 

The fact that you are presently reading this means that you have had
knowledge of the CeCILL-C license and that you accept its terms.
-------------------------------------------------------------------- */
/* -------------------------------------------------------- */
/*

LibSL shadow map tutorial

*/
/* -------------------------------------------------------- */

#include <windows.h>

/* -------------------------------------------------------- */

#include <LibSL/LibSL.h>
LIBSL_WIN32_FIX;

/* -------------------------------------------------------- */

#include <cstdio>
#include <cmath>
#include <iostream>
using namespace std;

#define Z_NEAR     0.1f // distance of near clipping plane
#define Z_FAR     10.0f // distance of far  clipping plane
#define FOV      (float(M_PI)/4.0f)
#define SCREEN_W 1024
#define SCREEN_H 1024

/* -------------------------------------------------------- */

int          g_MainWindow;                 // glut Window Id
int          g_W                  = 600;   // window width
int          g_H                  = 600;   // window height

bool         g_WireframeMode      = false; // enable/disable wireframe
float        g_Zoom               = 1.0f;  // zoom factor
int          g_MouseX             = 0;     // previous mouse coord in x
int          g_MouseY             = 0;     // previous mouse coord in y
bool         g_TrackView          = true;  // track view?
float        g_ObjectRotation     = 0.0f;  // object rotation angle
bool         g_StopObjectRotation = true;  // rotate object?
bool         g_UsePolygonOffset   = true;  // use polygon offset?
m4x4f        g_LightTransform;

RenderTarget2DDepth24_Ptr                              g_ShdwMap;

TriangleMesh_Ptr                                       g_Mesh;
AutoPtr<MeshRenderer<MeshFormat_3DS::t_VertexFormat> > g_Renderer;
AutoPtr<Shapes::Square>                                g_Square;

GLShader    g_glShadow;
GLParameter g_glLightView;
GLParameter g_glShdwMatrix;
GLParameter g_glShadowMap;

/* -------------------------------------------------------- */

void createShadowMap(int res)
{
  g_ShdwMap  = new RenderTarget2DDepth24(res,res);

  glBindTexture  (GL_TEXTURE_2D,g_ShdwMap->texture());
 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S    , GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T    , GL_CLAMP_TO_BORDER);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
//  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);

}

/* -------------------------------------------------------- */

void mainKeyboard(unsigned char key)
{
  if (key == 'q') {
    exit (0);
  } else if (key == ' ') {
    g_TrackView = !g_TrackView;
  } else if (key == '\\') {
    static bool swap = true;
    if (swap) {
      TrackballUI::trackball().setWalkthrough(true);
    } else {
      TrackballUI::trackball().setWalkthrough(false);
    }
    swap = !swap;
  }

}

/* -------------------------------------------------------- */

void renderShadowMap()
{

  g_ShdwMap ->bind();

  GPUHelpers::Renderer::setViewport (0,0,g_ShdwMap->w(),g_ShdwMap->h());

  GPUHelpers::clearScreen           (LIBSL_COLOR_BUFFER | LIBSL_DEPTH_BUFFER, 0,0,0);

  GPUHelpers::Transform::perspective(LIBSL_PROJECTION_MATRIX,FOV,1.0f,Z_NEAR,Z_FAR);
  GPUHelpers::Transform::set        (LIBSL_MODELVIEW_MATRIX ,g_LightTransform);

  glPolygonOffset(4.0f,4.0f);
	glEnable(GL_POLYGON_OFFSET_FILL);

  g_Square  ->render();
  g_Renderer->render();

  glDisable(GL_POLYGON_OFFSET_FILL);

  g_ShdwMap ->unbind();

}

/* -------------------------------------------------------- */

void mainRender()
{
  //////////////////////
  /// get trackball transform
  m4x4f trackball = TrackballUI::matrix();
  if (g_TrackView) {
    // track current view as light transform
    g_LightTransform = trackball;
  }

  //////////////////////
  /// render scene into the shadow map
  renderShadowMap();

  GPUHelpers::Renderer::setViewport(0,0,SCREEN_W,SCREEN_H);
  GPUHelpers::clearScreen(LIBSL_COLOR_BUFFER | LIBSL_DEPTH_BUFFER, 0.1,0.1,0.1);

  //////////////////////
  /// draw scene with shadow

  GPUHelpers::Transform::perspective(LIBSL_PROJECTION_MATRIX,FOV,1.0f,Z_NEAR,Z_FAR);
  GPUHelpers::Transform::set        (LIBSL_MODELVIEW_MATRIX,trackball);

  // set up matrices in GLSL
  g_glShadow    .begin();

  g_glLightView.set(g_LightTransform);

  // bind shadow map to unit 0
  glActiveTextureARB(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D,g_ShdwMap->texture());
  g_glShadowMap.set(GLuint(0));

  // render geometry
  g_Renderer->render();
  g_Square  ->render();

  g_glShadow    .end();

#if 1
  /// draw on-screen shadow map
  GPUHelpers::Renderer::setViewport(0,0,256,256);
  GPUHelpers::Transform::ortho2D (LIBSL_PROJECTION_MATRIX,-1.0f,1.0f,1.0f,-1.0f);
  GPUHelpers::Transform::identity(LIBSL_MODELVIEW_MATRIX);

  glPolygonMode  (GL_FRONT_AND_BACK,GL_FILL);
  glEnable       (GL_TEXTURE_2D);
  glBindTexture  (GL_TEXTURE_2D,g_ShdwMap->texture());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_NONE);
  g_Square   ->render();
  glDisable      (GL_TEXTURE_2D);
#endif

}

/* -------------------------------------------------------- */

void main(int argc, char **argv) 
{
  try {

    /// init simple UI (glut clone for both GL and D3D)
    cerr << "Init TrackballUI   ";
    TrackballUI::onRender     = mainRender;
    TrackballUI::onKeyPressed = mainKeyboard;
    TrackballUI::init(SCREEN_W,SCREEN_H);
    TrackballUI::setCenter(V3F(.5f,.5f,.5f));
    cerr << "[OK]" << endl;

    /// Create the shadow map
    createShadowMap(1024);

    // load mesh
    cerr << "Loading mesh      ";
    g_Mesh     = loadTriangleMesh("torus.3ds");
    g_Mesh->scaleToUnitCube();
    g_Mesh->centerOn(V3F(0,0,0.5));
    cerr << "[OK]" << endl;
    cerr << sprint("  mesh contains %d vertices and %d triangles\n",g_Mesh->numVertices(),g_Mesh->numTriangles());
    cerr << "Creating renderer ";
    g_Renderer = new MeshRenderer<MeshFormat_3DS::t_VertexFormat>(g_Mesh);
    cerr << "[OK]" << endl;

    glDisable(GL_LIGHTING);
    glColor3f(1,1,1);

    // create a square
    g_Square = new Shapes::Square();

    // shader
    string code_vp = loadFileIntoString("shadow.vp");
    string code_fp = loadFileIntoString("shadow.fp");
    g_glShadow    .init(code_vp.c_str(),code_fp.c_str());
    g_glShadow    .setStrict(false);
    g_glLightView .init(g_glShadow,"u_LightView");
    g_glShdwMatrix.init(g_glShadow,"u_ShdwMatrix");
    g_glShadowMap .init(g_glShadow,"u_ShadowMap");
    
    // set shadow transform
    float fOffsetX = 0.5f;// + ( 0.5f / float(g_ShdwMap->w()) );
    float fOffsetY = 0.5f;// + ( 0.5f / float(g_ShdwMap->h()) );
    m4x4f shadowTransform(
      0.5f,     0.0f,     0.0f,         fOffsetX,
      0.0f,     0.5f,     0.0f,         fOffsetY,
      0.0f,     0.0f,     0.5f,         0.5f,
      0.0f,     0.0f,     0.0f,         1.0f );
    g_glShadow    .begin();
    g_glShdwMatrix.set(shadowTransform);
    g_glShadow    .end();

    // print a small documentation
    printf("[q]     - quit\n");

    /// main loop
    TrackballUI::loop();

    /// clean exit
    g_Mesh         = NULL;
    g_Renderer     = NULL;
    g_Square       = NULL;
    g_ShdwMap      = NULL;
    g_glShadow.terminate();

    // shutdown SimpleUI
    TrackballUI::shutdown();

  } catch (Fatal& e) {
    cerr << e.message() << endl;
  }

}

/* -------------------------------------------------------- */
