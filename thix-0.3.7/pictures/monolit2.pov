
/*
 * GNU Interactive Tools
 * Created: August 25, 1995 by Tudor Hulubei
 * Rendered with POV-Ray 2.2
 */


// WARNING:  THIS PICTURE SHOULD BE RENDERED AT A MINIMUM OF 640x480.


#include "colors.inc"
#include "shapes.inc"
#include "textures.inc"

camera
{
    location <0, 1.5, -3>
    look_at <0.35, 0.6, 0.12>
}

background
{
    color Gray
}

plane
{
    y, 0
    normal
    {
	waves 0.5
	frequency 5000
	scale 3000.0
    }
    pigment
    {
	MediumGoldenrod
    }
    finish
    {
	reflection 0.8
    }
}

box
{
    <0, 0, 0>,		// Near lower left corner
    <0.6, 1.3, 0.3>	// Far upper right corner
    texture
    {
	pigment
	{
	    color Black
	}
	finish
	{
	    phong 1 metallic
	    reflection 0.3
	    refraction 1
	    ior 2.4
	}
    }
    rotate y*-10
}

light_source
{
    <0, 1.5, -3>
    color White
}
