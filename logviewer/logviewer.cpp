// logviewer.cpp

#include <GL/glut.h>
#include <vector>
#include <unistd.h>

#include "logviewer.h"
#include "robot.h"
#include "home.h"
#include "puckstack.h"

int Logviewer::winsize = 600;
double Logviewer::worldsize = 1.0;
vector<Robot*> Logviewer::robots;
vector<Home*> Logviewer::homes;
vector<PuckStack*> Logviewer::puckStacks;

Logviewer::Logviewer(int worldLength, int worldHeight) 
  : _worldLength(worldLength), _worldHeight(worldHeight) {
  // Initialize window size to top left corner 
  _winHeight = 500;
  _winLength = 500;
  _winStartX = 0;
  _winStartY = 0;
}

void Logviewer::getInitialData() {
  // from config file?
  // Hardcode for now...

  // determine which indexes correspond with which robots
  int robotsPerTeam = 5;
  int teams = 3;
  int pucks = 20;

  // Now parse initial world data to assign robots to vector
  Robot* tempRobot = NULL;
  Home* tempHome = NULL;
  PuckStack* tempPuckStack = NULL;
  for (int i = 0; i < teams; i++) {
    tempHome = new Home(1.0 / (i + 2), 1.0 / (i + 2), 0.2, i);
    Logviewer::homes.push_back(tempHome);
    for (int j = 0; j < robotsPerTeam; j++) {
      tempRobot = new Robot(1.0 / (i + j + 2), 1.0 / (i + j + 2), i); // arbitrary position
      robots.push_back(tempRobot); 
    }
  }

  for (int i = 0; i < pucks; i++) {
    tempPuckStack = new PuckStack(1.0 / (i + 2), 0.5, 1);
    puckStacks.push_back(tempPuckStack);
  }
}

void Logviewer::updateTimestep() {
  // 1. Parse ServerRobot and PuckStack messages for current timestep.
  // 2. For all robots we did not receive a message for, calculate new
  //    x,y coordinates based on current velocity.

  // But until we get log files... let's make them DANCE!
  double dx, dy;
  Robot* bot = NULL;
  for (int i = 0; i < robots.size(); i++) {
    bot = robots.at(i);
    bot->_velocity = 0.0001;
    dx = bot->_velocity * cos(bot->_angle);
    dy = bot->_velocity * sin(bot->_angle);
    bot->_x = distanceNormalize(bot->_x + dx); 
    bot->_y = distanceNormalize(bot->_y + dy); 
  }
  usleep(0.1);
  return;
}


// GUI**********************

static void idle_func( void )
{
  Logviewer::updateTimestep();
}

static void timer_func( int dummy )
{
  glutPostRedisplay(); // force redraw
}

// draw the world - this is called whenever the window needs redrawn
static void display_func( void ) 
{  
  Logviewer::winsize = glutGet( GLUT_WINDOW_WIDTH );
  glClear( GL_COLOR_BUFFER_BIT );  
  Logviewer::drawAll();
  glutSwapBuffers();
	
  // run this function again in about 50 msec
  glutTimerFunc( 20, timer_func, 0 );
}

static void mouse_func(int button, int state, int x, int y) 
{  
  /*if( (button == GLUT_LEFT_BUTTON) && (state == GLUT_DOWN ) )
	 {
		Robot::paused = !Robot::paused;
	 }
   */
}


// utility
void glDrawCircle( double x, double y, double r, double count )
{
	glBegin(GL_LINE_LOOP);
	for( float a=0; a<(M_PI*2.0); a+=M_PI/count )
		glVertex2f( x + sin(a) * r, y + cos(a) * r );
	glEnd();
}


//
// Robot static member methods ---------------------------------------------

void Logviewer::initGraphics( int argc, char* argv[] )
{
  // initialize opengl graphics
  glutInit( &argc, argv );
  glutInitWindowSize( winsize, winsize );
  glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGBA );
  glutCreateWindow( argv[0] ); // program name
  glClearColor( 0.1,0.1,0.1,1 ); // dark grey
  glutDisplayFunc( display_func );
  glutTimerFunc( 50, timer_func, 0 );
  glutMouseFunc( mouse_func );
  glutIdleFunc( idle_func );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glEnable( GL_BLEND );
  glMatrixMode( GL_PROJECTION );
  glLoadIdentity();
  gluOrtho2D( 0,1,0,1 );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
  glScalef( 1.0/worldsize, 1.0/worldsize, 1 ); 
	glPointSize( 4.0 );
}

void Logviewer::updateGui()
{
	glutMainLoop();
}

// render all robots in OpenGL
void Logviewer::drawAll()
{		
	FOR_EACH( r, robots )
		draw(*r);
	
	FOR_EACH( it, Logviewer::homes )
		{
			Home* h = *it;
			
			glColor3f( 1, 1, 1);

			glDrawCircle( h->_x, h->_y, h->_radius, 16 );
			glDrawCircle( h->_x+worldsize, h->_y, h->_radius, 16 );
			glDrawCircle( h->_x-worldsize, h->_y, h->_radius, 16 );
			glDrawCircle( h->_x, h->_y+worldsize, h->_radius, 16 );
			glDrawCircle( h->_x, h->_y-worldsize, h->_radius, 16 );
		}
	
	glColor3f( 1,1,1 ); // green
	glBegin( GL_POINTS );
	FOR_EACH( p, puckStacks )
		glVertex2f( (*p)->_x, (*p)->_y );
	glEnd();
}

// draw a robot
void Logviewer::draw(Robot* r)
{
  glPushMatrix();
	// shift into this robot's local coordinate frame
  glTranslatef( r->_x, r->_y, 0 );
  glRotatef( rtod(r->_angle), 0,0,1 );
  
	glColor3f( 1, 1, 1); 
	
	double radius = .01; //Robot::radius;
	
	// if robots are smaller than 4 pixels across, draw them as points
	if( (radius * (double)winsize/(double)worldsize) < 2.0 )
	  {
		 glBegin( GL_POINTS );
		 glVertex2f( 0,0 );
		 glEnd();
	  }
	else
	  {
		 // draw a circular body
		 glBegin(GL_LINE_LOOP);
		 for( float a=0; a<(M_PI*2.0); a+=M_PI/16 )
			glVertex2f( sin(a) * radius, 
							cos(a) * radius );
		 glEnd();
		 
		 // draw a nose indicating forward direction
		 glBegin(GL_LINES);
		 glVertex2f( 0, 0 );
		 glVertex2f( radius, 0 );
		 glEnd();
	  }

  /*if( Robot::show_data )
	 {
		glColor3f( 1,0,0 ); // red
		
		FOR_EACH( it, see_robots )
		  {
				float dx = it->range * cos(it->bearing);
				float dy = it->range * sin(it->bearing);
				
				glBegin( GL_LINES );
				glVertex2f( 0,0 );
				glVertex2f( dx, dy );
				glEnd();
		  }
		
		glColor3f( 0.3,0.8,0.3 ); // light green
		
		FOR_EACH( it, see_pucks )
		  {
				float dx = it->range * cos(it->bearing);
				float dy = it->range * sin(it->bearing);
				
				glBegin( GL_LINES );
				glVertex2f( 0,0 );
				glVertex2f( dx, dy );
				glEnd();
		  }
		
		glColor3f( 0.4,0.4,0.4 ); // grey

		// draw the sensor FOV
		glBegin(GL_LINE_LOOP);
		
		glVertex2f( 0, 0 );
		
		double right = -fov/2.0;
		double left = +fov/2.0;// + M_PI;
		double incr = fov/32.0;
		for( float a=right; a<left; a+=incr)
		  glVertex2f( cos(a) * range, 
						  sin(a) * range );

		glVertex2f( cos(left) * range, 
						sin(left) * range );
		
		glEnd();		
	 }*/
	
	// shift out of local coordinate frame
  glPopMatrix();
}

/** Normalize a length to within 0 to worldsize. */
double Logviewer::distanceNormalize(double d) {
	while( d < 0 ) d += worldsize;
	while( d > worldsize ) d -= worldsize;
	return d; 
} 

/** Normalize an angle to within +/_ M_PI. */
double Logviewer::angleNormalize(double a) {
	while( a < -M_PI ) a += 2.0*M_PI;
	while( a >  M_PI ) a -= 2.0*M_PI;	 
	return a;
}	 
