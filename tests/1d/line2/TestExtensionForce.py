#!/usr/bin/env python
#
# ----------------------------------------------------------------------
#
#                           Brad T. Aagaard
#                        U.S. Geological Survey
#
# <LicenseText>
#
# ----------------------------------------------------------------------
#

## @file tests/1d/line2/TestExtensionForce.py
##
## @brief Test suite for testing pylith with 1-D axial extension using
## point forces.

import numpy
from TestLine2 import TestLine2
from pylith.utils.VTKDataReader import has_vtk
from pylith.utils.VTKDataReader import VTKDataReader


# Local version of PyLithApp
from pylith.apps.PyLithApp import PyLithApp
class AxialApp(PyLithApp):
  def __init__(self):
    PyLithApp.__init__(self, name="extensionforce")
    return


# Helper function to run PyLith
def run_pylith():
  """
  Run pylith.
  """
  if not "done" in dir(run_pylith):
    app = AxialApp()
    app.run()
    run_pylith.done = True
  return


class TestExtensionForce(TestLine2):
  """
  Test suite for testing pylith with 1-D axial extension with point forces.
  """

  def setUp(self):
    """
    Setup for test.
    """
    TestLine2.setUp(self)
    run_pylith()
    self.outputRoot = "extensionforce"
    if has_vtk():
      self.reader = VTKDataReader()
    else:
      self.reader = None
    return


  def calcDisplacements(self, vertices):
    """
    Calculate displacement field given coordinates of vertices.
    """
    nvertices = self.mesh['nvertices']
    spaceDim = self.mesh['spaceDim']    
    disp = numpy.zeros( (nvertices, spaceDim), dtype=numpy.float64)
    disp[:,0] = 3.0/7.0 * (-2.0 + vertices[:,0])

    return disp


  def calcStateVar(self, name, vertices, cells):
    """
    Calculate state variable.
    """
    exx = 3.0/7.0

    ncells = self.mesh['ncells']
    tensorSize = self.mesh['tensorSize']

    if name == "total_strain":
      stateVar = exx*numpy.ones( (ncells, tensorSize), dtype=numpy.float64)
    
    elif name == "stress":
      lp2m = self.density*self.vp**2
      stateVar = lp2m*exx * numpy.ones( (ncells, tensorSize), 
                                       dtype=numpy.float64)
    else:
      raise ValueError("Unknown state variable '%s'." % name)

    return stateVar


# End of file 
