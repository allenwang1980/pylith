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

## @file pylith/feassemble/FIATCell.py
##
## @brief Python object for managing basis functions and quadrature
## rules of a reference finite-element cell using FIAT.
##
## Factory: reference_cell.

from ReferenceCell import ReferenceCell

import numpy

# FIATCell class
class FIATCell(ReferenceCell):
  """
  Python object for managing basis functions and quadrature rules of a
  reference finite-element cell using FIAT.

  Factory: reference_cell.
  """

  # PUBLIC METHODS /////////////////////////////////////////////////////

  def __init__(self, name="fiatcell"):
    """
    Constructor.
    """
    ReferenceCell.__init__(self, name)
    return


  def initialize(self):
    """
    Initialize reference finite-element cell.
    """
    quadrature = self._setupQuadrature()
    basisFns = self._setupBasisFns()
    vertices = self._setupVertices()
    
    # Evaluate basis functions at vertices
    basis = numpy.array(basisFns.tabulate(vertices)).transpose()
    self.basisVert = numpy.reshape(basis.flatten(), basis.shape)

    # Evaluate derivatives of basis functions at vertices
    import FIAT.shapes
    dim = FIAT.shapes.dimension(basisFns.base.shape)
    basisDeriv = numpy.array([basisFns.deriv_all(d).tabulate(vertices) \
                              for d in range(dim)]).transpose()
    self.basisDerivVert = numpy.reshape(basisDeriv.flatten(), basisDeriv.shape)

    # Evaluate basis functions at quadrature points
    quadpts = quadrature.get_points()
    basis = numpy.array(basisFns.tabulate(quadpts)).transpose()
    self.basisQuad = numpy.reshape(basis.flatten(), basis.shape)

    # Evaluate derivatives of basis functions at quadrature points
    import FIAT.shapes
    dim = FIAT.shapes.dimension(basisFns.base.shape)
    basisDeriv = numpy.array([basisFns.deriv_all(d).tabulate(quadpts) \
                              for d in range(dim)]).transpose()
    self.basisDerivQuad = numpy.reshape(basisDeriv.flatten(), basisDeriv.shape)

    self.quadPts = numpy.array(quadrature.get_points())
    self.quadWts = numpy.array(quadrature.get_weights())

    self.cellDim = dim
    self.numCorners = self.basisVert.shape[1]
    self.numQuadPts = len(quadrature.get_weights())

    self._info.line("Basis (vertices):")
    self._info.line(self.basisVert)
    self._info.line("Basis derivatives (vertices):")
    self._info.line(self.basisDerivVert)
    self._info.line("Basis (quad pts):")
    self._info.line(self.basisQuad)
    self._info.line("Basis derivatives (quad pts):")
    self._info.line(self.basisDerivQuad)
    self._info.line("Quad pts:")
    self._info.line(quadrature.get_points())
    self._info.line("Quad wts:")
    self._info.line(quadrature.get_weights())

    self._info.log()

    return


  # PRIVATE METHODS ////////////////////////////////////////////////////

  def _setupQuadrature(self):
    """
    Setup quadrature rule for reference cell.
    """
    raise NotImplementedError()
    return


  def _setupBasisFns(self):
    """
    Setup basis functions for reference cell.
    """
    raise NotImplementedError()
    return


  def _setupVertices(self):
    """
    Setup vertices for reference cell.
    """
    raise NotImplementedError()
    return


# End of file 
