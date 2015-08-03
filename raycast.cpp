#include "framework.h"
#include "tbb/tbb.h"

using namespace prototyper;

framework frm;

void write_backbuffer( const vec2& pos, const vec4& color );

#ifndef EPSILON
#define EPSILON 0.001f
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38
#endif

#define INVALID (FLT_MAX)

//////////////////////////////////////////////////
// Code that runs the raytracer
//////////////////////////////////////////////////

uvec2 screen( 0 );
bool fullscreen = false;
bool silent = false;
string title = "Voxel rendering stuff";
vec4* pixels = 0;

void write_backbuffer( const vec2& pos, const vec4& color )
{
  assert( pos.x < screen.x && pos.y < screen.y && pos.x >= 0 && screen.y >= 0 );

  pixels[int(pos.y * screen.x + pos.x)] = color;
}

void drawCircle( int res, const vec3& pos, float radius, const vec3& color )
{
  glColor3f(color.x, color.y, color.z);

  glBegin( GL_LINE_STRIP );

  for( int c = 0; c <= res; ++c )
  {
    vec3 p = pos;
    float t = float(c) / res;
    p.x += radius * cosf( 2 * pi * t );
    p.y += radius * sinf( 2 * pi * t ); 

    glVertex3f( p.x, p.y, p.z );
  }

  glEnd();
}

int main( int argc, char** argv )
{
  shape::set_up_intersection();

  map<string, string> args;

  for( int c = 1; c < argc; ++c )
  {
    args[argv[c]] = c + 1 < argc ? argv[c + 1] : "";
    ++c;
  }

  cout << "Arguments: " << endl;
  for_each( args.begin(), args.end(), []( pair<string, string> p )
  {
    cout << p.first << " " << p.second << endl;
  } );

  /*
   * Process program arguments
   */

  stringstream ss;
  ss.str( args["--screenx"] );
  ss >> screen.x;
  ss.clear();
  ss.str( args["--screeny"] );
  ss >> screen.y;
  ss.clear();

  if( screen.x == 0 )
  {
    screen.x = 512;
  }

  if( screen.y == 0 )
  {
    screen.y = 512;
  }

  try
  {
    args.at( "--fullscreen" );
    fullscreen = true;
  }
  catch( ... ) {}

  try
  {
    args.at( "--help" );
    cout << title << ", written by Marton Tamas." << endl <<
         "Usage: --silent      //don't display FPS info in the terminal" << endl <<
         "       --screenx num //set screen width (default:1280)" << endl <<
         "       --screeny num //set screen height (default:720)" << endl <<
         "       --fullscreen  //set fullscreen, windowed by default" << endl <<
         "       --help        //display this information" << endl;
    return 0;
  }
  catch( ... ) {}

  try
  {
    args.at( "--silent" );
    silent = true;
  }
  catch( ... ) {}

  /*
   * Initialize the OpenGL context
   */

  frm.init( screen, title, fullscreen );
  frm.set_vsync(true);

  //set opengl settings
  glEnable( GL_DEPTH_TEST );
  glDepthFunc( GL_LEQUAL );
  glFrontFace( GL_CCW );
  glEnable( GL_CULL_FACE );
  glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
  glClearDepth( 1.0f );

  frm.get_opengl_error();

  glViewport( 0, 0, screen.x, screen.y );
  mat4 oo = ortographic( 0.0f, (float)screen.x, 0.0f, (float)screen.y, 0.0f, 1.0f );
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(&oo[0][0]);

  /*
   * Set up the scene
   */

   pixels = new vec4[screen.x * screen.y];

  //initialize thread building blocks
  tbb::task_scheduler_init();

  /*
   * Handle events
   */

  vec2 mousexy;

  auto event_handler = [&]( const sf::Event & ev )
  {
    switch( ev.type )
    {
      case sf::Event::MouseMoved:
        {
          mousexy.x = ev.mouseMove.x;
		      mousexy.y = screen.y - ev.mouseMove.y;
        }
      default:
        break;
    }
  };

  /*
   * Render
   */

  sf::Clock timer;
  timer.restart();

  cout << "Init finished, rendering starts..." << endl;

  frm.display( [&]
  {
    frm.handle_events( event_handler );

    memset(pixels, 0, screen.x * screen.y * sizeof(vec4));

    /**/
    //64x64 world, displayed onto a 512x512 screen
    float world_size = 64;
	  float upscale = 512 / world_size;
    vec2 ro = vec2( 16, 16 ) + vec2( 0.5, 0 );
    //vec2 rd = normalize( vec2(1, 0.5) );
    vec2 rd = normalize(mousexy/upscale - ro);
    float len = 30;
    vec2 rf = ro + rd * len;

    vec2 XYZ = floor(ro);
    ro += EPSILON;
    vec2 stepXYZ, deltaXYZ, nextXYZ;

    for( int c = 0; c < 2; ++c )
    {
      stepXYZ[c] = rd[c] < 0 ? -1 : 1;
    }

  #ifdef _DEBUG
    //in debug mode, pay attention to asserts
    for(int c = 0; c < 2; ++c )
    {
      if( mm::impl::is_eq(rd[c], 0) )
      {
        deltaXYZ[c] = FLT_MAX;
      }
      else
      {
        deltaXYZ[c] = stepXYZ[c] / rd[c];
      }
    }
  #else
    //in release mode we dgaf about div by zero
    deltaXYZ = stepXYZ / rd;
  #endif
    nextXYZ = -stepXYZ * (ro - XYZ) * deltaXYZ + max(stepXYZ, 0) * deltaXYZ;

    vec2 mask;

    while(XYZ.x < world_size &&
		      XYZ.y < world_size &&
          XYZ.x >= 0 &&
          XYZ.y >= 0 )
    {
      for(int y = XYZ.y * upscale; y < (XYZ.y+1)*upscale; ++y )
        for(int x = XYZ.x * upscale; x < (XYZ.x+1)*upscale; ++x )
          write_backbuffer( vec2(x, y), vec4(1,0,0,1) );

      if(nextXYZ.x < nextXYZ.y)
        mask = vec2(1,0);
      else
        mask = vec2(0,1);

      //update next
      nextXYZ += mask * deltaXYZ;
      //move into that direction
      XYZ += mask * stepXYZ;
    }

    /**/

    glDrawPixels( screen.x, screen.y, GL_RGBA, GL_FLOAT, pixels );

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    //draw grid
    glColor3f(0.2, 0.2, 0.2);
    for(int y = 0; y < world_size; ++y)
    {
      glBegin(GL_LINES);
      glVertex2f(0, y*upscale);
      glVertex2f(world_size*upscale, y*upscale);
      glEnd();
    }

    for(int x = 0; x < world_size; ++x)
    {
      glBegin(GL_LINES);
      glVertex2f(x*upscale, 0);
      glVertex2f(x*upscale, world_size*upscale);
      glEnd();
    }

    //draw vector
    drawCircle( 10, vec3( ro*upscale, 0 ), 5, 1 );

    glBegin(GL_LINES);
    glVertex2f(ro.x*upscale, ro.y*upscale);
    glVertex2f(rf.x*upscale, rf.y*upscale);
    glEnd();

    glDisable(GL_BLEND);

    //cout << "frame" << endl;

    frm.get_opengl_error();
  }, silent );

  return 0;
}
