// ---------------------------------------------------------------------
//
// Copyright (C) 2020 - 2022 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------

#include <deal.II/base/polynomial.h>
#include <deal.II/base/polynomials_barycentric.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/tensor_product_polynomials.h>

#include <deal.II/fe/fe_pyramid_p.h>
#include <deal.II/fe/fe_simplex_p.h>
#include <deal.II/fe/fe_simplex_p_bubbles.h>
#include <deal.II/fe/fe_wedge_p.h>
#include <deal.II/fe/mapping_fe.h>
#include <deal.II/fe/mapping_q.h>
#include <deal.II/fe/mapping_q1.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/reference_cell.h>
#include <deal.II/grid/tria.h>

#include <iostream>
#include <memory>

DEAL_II_NAMESPACE_OPEN

namespace
{
  namespace VTKCellType
  {
    // Define VTK constants for linear, quadratic and
    // high-order Lagrange geometrices
    enum : unsigned int
    {
      VTK_VERTEX = 1,
      // Linear cells
      VTK_LINE       = 3,
      VTK_TRIANGLE   = 5,
      VTK_QUAD       = 9,
      VTK_TETRA      = 10,
      VTK_HEXAHEDRON = 12,
      VTK_WEDGE      = 13,
      VTK_PYRAMID    = 14,
      // Quadratic cells
      VTK_QUADRATIC_EDGE       = 21,
      VTK_QUADRATIC_TRIANGLE   = 22,
      VTK_QUADRATIC_QUAD       = 23,
      VTK_QUADRATIC_TETRA      = 24,
      VTK_QUADRATIC_HEXAHEDRON = 25,
      VTK_QUADRATIC_WEDGE      = 26,
      VTK_QUADRATIC_PYRAMID    = 27,
      // Lagrange cells
      VTK_LAGRANGE_CURVE         = 68,
      VTK_LAGRANGE_TRIANGLE      = 69,
      VTK_LAGRANGE_QUADRILATERAL = 70,
      VTK_LAGRANGE_TETRAHEDRON   = 71,
      VTK_LAGRANGE_HEXAHEDRON    = 72,
      VTK_LAGRANGE_WEDGE         = 73,
      VTK_LAGRANGE_PYRAMID       = 74,
      // Invalid code
      VTK_INVALID = numbers::invalid_unsigned_int
    };

  } // namespace VTKCellType

} // namespace

constexpr ndarray<unsigned int, 2, 2> ReferenceCell::line_vertex_permutations;

constexpr ndarray<unsigned int, 6, 3>
  ReferenceCell::triangle_vertex_permutations;

constexpr ndarray<unsigned int, 8, 4>
  ReferenceCell::quadrilateral_vertex_permutations;

std::string
ReferenceCell::to_string() const
{
  switch (this->kind)
    {
      case ReferenceCells::Vertex:
        return "Vertex";
      case ReferenceCells::Line:
        return "Line";
      case ReferenceCells::Triangle:
        return "Tri";
      case ReferenceCells::Quadrilateral:
        return "Quad";
      case ReferenceCells::Tetrahedron:
        return "Tet";
      case ReferenceCells::Pyramid:
        return "Pyramid";
      case ReferenceCells::Wedge:
        return "Wedge";
      case ReferenceCells::Hexahedron:
        return "Hex";
      case ReferenceCells::Invalid:
        return "Invalid";
      default:
        Assert(false, ExcNotImplemented());
    }

  return "Invalid";
}



template <int dim, int spacedim>
std::unique_ptr<Mapping<dim, spacedim>>
ReferenceCell::get_default_mapping(const unsigned int degree) const
{
  AssertDimension(dim, get_dimension());

  if (is_hyper_cube())
    return std::make_unique<MappingQ<dim, spacedim>>(degree);
  else if (is_simplex())
    return std::make_unique<MappingFE<dim, spacedim>>(
      FE_SimplexP<dim, spacedim>(degree));
  else if (*this == ReferenceCells::Pyramid)
    return std::make_unique<MappingFE<dim, spacedim>>(
      FE_PyramidP<dim, spacedim>(degree));
  else if (*this == ReferenceCells::Wedge)
    return std::make_unique<MappingFE<dim, spacedim>>(
      FE_WedgeP<dim, spacedim>(degree));
  else
    {
      Assert(false, ExcNotImplemented());
    }

  return std::make_unique<MappingQ<dim, spacedim>>(degree);
}



template <int dim, int spacedim>
const Mapping<dim, spacedim> &
ReferenceCell::get_default_linear_mapping() const
{
  AssertDimension(dim, get_dimension());

  if (is_hyper_cube())
    {
      return StaticMappingQ1<dim, spacedim>::mapping;
    }
  else if (is_simplex())
    {
      static const MappingFE<dim, spacedim> mapping(
        FE_SimplexP<dim, spacedim>(1));
      return mapping;
    }
  else if (*this == ReferenceCells::Pyramid)
    {
      static const MappingFE<dim, spacedim> mapping(
        FE_PyramidP<dim, spacedim>(1));
      return mapping;
    }
  else if (*this == ReferenceCells::Wedge)
    {
      static const MappingFE<dim, spacedim> mapping(
        FE_WedgeP<dim, spacedim>(1));
      return mapping;
    }
  else
    {
      Assert(false, ExcNotImplemented());
    }

  return StaticMappingQ1<dim, spacedim>::mapping; // never reached
}



template <int dim>
Quadrature<dim>
ReferenceCell::get_gauss_type_quadrature(const unsigned n_points_1D) const
{
  AssertDimension(dim, get_dimension());

  if (is_hyper_cube())
    return QGauss<dim>(n_points_1D);
  else if (is_simplex())
    return QGaussSimplex<dim>(n_points_1D);
  else if (*this == ReferenceCells::Pyramid)
    return QGaussPyramid<dim>(n_points_1D);
  else if (*this == ReferenceCells::Wedge)
    return QGaussWedge<dim>(n_points_1D);
  else
    Assert(false, ExcNotImplemented());

  return Quadrature<dim>(); // never reached
}



template <int dim>
const Quadrature<dim> &
ReferenceCell::get_nodal_type_quadrature() const
{
  AssertDimension(dim, get_dimension());

  // A function that is used to fill a quadrature object of the
  // desired type the first time we encounter a particular
  // reference cell
  const auto create_quadrature = [](const ReferenceCell &reference_cell) {
    std::vector<Point<dim>> vertices(reference_cell.n_vertices());
    for (const unsigned int v : reference_cell.vertex_indices())
      vertices[v] = reference_cell.vertex<dim>(v);

    return Quadrature<dim>(vertices);
  };

  if (is_hyper_cube())
    {
      static const Quadrature<dim> quadrature = create_quadrature(*this);
      return quadrature;
    }
  else if (is_simplex())
    {
      static const Quadrature<dim> quadrature = create_quadrature(*this);
      return quadrature;
    }
  else if (*this == ReferenceCells::Pyramid)
    {
      static const Quadrature<dim> quadrature = create_quadrature(*this);
      return quadrature;
    }
  else if (*this == ReferenceCells::Wedge)
    {
      static const Quadrature<dim> quadrature = create_quadrature(*this);
      return quadrature;
    }
  else
    Assert(false, ExcNotImplemented());

  static const Quadrature<dim> dummy;
  return dummy; // never reached
}



unsigned int
ReferenceCell::exodusii_vertex_to_deal_vertex(const unsigned int vertex_n) const
{
  AssertIndexRange(vertex_n, n_vertices());

  switch (this->kind)
    {
      case ReferenceCells::Line:
      case ReferenceCells::Triangle:
        return vertex_n;
      case ReferenceCells::Quadrilateral:
        {
          constexpr std::array<unsigned int, 4> exodus_to_deal{{0, 1, 3, 2}};
          return exodus_to_deal[vertex_n];
        }
      case ReferenceCells::Tetrahedron:
        return vertex_n;
      case ReferenceCells::Hexahedron:
        {
          constexpr std::array<unsigned int, 8> exodus_to_deal{
            {0, 1, 3, 2, 4, 5, 7, 6}};
          return exodus_to_deal[vertex_n];
        }
      case ReferenceCells::Wedge:
        {
          constexpr std::array<unsigned int, 6> exodus_to_deal{
            {2, 1, 0, 5, 4, 3}};
          return exodus_to_deal[vertex_n];
        }
      case ReferenceCells::Pyramid:
        {
          constexpr std::array<unsigned int, 5> exodus_to_deal{{0, 1, 3, 2, 4}};
          return exodus_to_deal[vertex_n];
        }
      default:
        Assert(false, ExcNotImplemented());
    }

  return numbers::invalid_unsigned_int;
}



unsigned int
ReferenceCell::exodusii_face_to_deal_face(const unsigned int face_n) const
{
  AssertIndexRange(face_n, n_faces());

  switch (this->kind)
    {
      case ReferenceCells::Vertex:
        return 0;
      case ReferenceCells::Line:
      case ReferenceCells::Triangle:
        return face_n;
      case ReferenceCells::Quadrilateral:
        {
          constexpr std::array<unsigned int, 4> exodus_to_deal{{2, 1, 3, 0}};
          return exodus_to_deal[face_n];
        }
      case ReferenceCells::Tetrahedron:
        {
          constexpr std::array<unsigned int, 4> exodus_to_deal{{1, 3, 2, 0}};
          return exodus_to_deal[face_n];
        }
      case ReferenceCells::Hexahedron:
        {
          constexpr std::array<unsigned int, 6> exodus_to_deal{
            {2, 1, 3, 0, 4, 5}};
          return exodus_to_deal[face_n];
        }
      case ReferenceCells::Wedge:
        {
          constexpr std::array<unsigned int, 6> exodus_to_deal{{3, 4, 2, 0, 1}};
          return exodus_to_deal[face_n];
        }
      case ReferenceCells::Pyramid:
        {
          constexpr std::array<unsigned int, 5> exodus_to_deal{{3, 2, 4, 1, 0}};
          return exodus_to_deal[face_n];
        }
      default:
        Assert(false, ExcNotImplemented());
    }

  return numbers::invalid_unsigned_int;
}



unsigned int
ReferenceCell::unv_vertex_to_deal_vertex(const unsigned int vertex_n) const
{
  AssertIndexRange(vertex_n, n_vertices());
  // Information on this file format isn't easy to find - the documents here
  //
  // https://www.ceas3.uc.edu/sdrluff/
  //
  // Don't actually explain anything about the sections we care about (2412) in
  // any detail. For node numbering I worked backwards from what is actually in
  // our test files (since that's supposed to work), which all use some
  // non-standard clockwise numbering scheme which starts at the bottom right
  // vertex.
  if (*this == ReferenceCells::Line)
    {
      return vertex_n;
    }
  else if (*this == ReferenceCells::Quadrilateral)
    {
      constexpr std::array<unsigned int, 4> unv_to_deal{{1, 0, 2, 3}};
      return unv_to_deal[vertex_n];
    }
  else if (*this == ReferenceCells::Hexahedron)
    {
      constexpr std::array<unsigned int, 8> unv_to_deal{
        {6, 7, 5, 4, 2, 3, 1, 0}};
      return unv_to_deal[vertex_n];
    }

  Assert(false, ExcNotImplemented());

  return numbers::invalid_unsigned_int;
}



unsigned int
ReferenceCell::vtk_linear_type() const
{
  switch (this->kind)
    {
      case ReferenceCells::Vertex:
        return VTKCellType::VTK_VERTEX;
      case ReferenceCells::Line:
        return VTKCellType::VTK_LINE;
      case ReferenceCells::Triangle:
        return VTKCellType::VTK_TRIANGLE;
      case ReferenceCells::Quadrilateral:
        return VTKCellType::VTK_QUAD;
      case ReferenceCells::Tetrahedron:
        return VTKCellType::VTK_TETRA;
      case ReferenceCells::Pyramid:
        return VTKCellType::VTK_PYRAMID;
      case ReferenceCells::Wedge:
        return VTKCellType::VTK_WEDGE;
      case ReferenceCells::Hexahedron:
        return VTKCellType::VTK_HEXAHEDRON;
      case ReferenceCells::Invalid:
        return VTKCellType::VTK_INVALID;
      default:
        Assert(false, ExcNotImplemented());
    }

  return VTKCellType::VTK_INVALID;
}



unsigned int
ReferenceCell::vtk_quadratic_type() const
{
  switch (this->kind)
    {
      case ReferenceCells::Vertex:
        return VTKCellType::VTK_VERTEX;
      case ReferenceCells::Line:
        return VTKCellType::VTK_QUADRATIC_EDGE;
      case ReferenceCells::Triangle:
        return VTKCellType::VTK_QUADRATIC_TRIANGLE;
      case ReferenceCells::Quadrilateral:
        return VTKCellType::VTK_QUADRATIC_QUAD;
      case ReferenceCells::Tetrahedron:
        return VTKCellType::VTK_QUADRATIC_TETRA;
      case ReferenceCells::Pyramid:
        return VTKCellType::VTK_QUADRATIC_PYRAMID;
      case ReferenceCells::Wedge:
        return VTKCellType::VTK_QUADRATIC_WEDGE;
      case ReferenceCells::Hexahedron:
        return VTKCellType::VTK_QUADRATIC_HEXAHEDRON;
      case ReferenceCells::Invalid:
        return VTKCellType::VTK_INVALID;
      default:
        Assert(false, ExcNotImplemented());
    }

  return VTKCellType::VTK_INVALID;
}



unsigned int
ReferenceCell::vtk_lagrange_type() const
{
  switch (this->kind)
    {
      case ReferenceCells::Vertex:
        return VTKCellType::VTK_VERTEX;
      case ReferenceCells::Line:
        return VTKCellType::VTK_LAGRANGE_CURVE;
      case ReferenceCells::Triangle:
        return VTKCellType::VTK_LAGRANGE_TRIANGLE;
      case ReferenceCells::Quadrilateral:
        return VTKCellType::VTK_LAGRANGE_QUADRILATERAL;
      case ReferenceCells::Tetrahedron:
        return VTKCellType::VTK_LAGRANGE_TETRAHEDRON;
      case ReferenceCells::Pyramid:
        return VTKCellType::VTK_LAGRANGE_PYRAMID;
      case ReferenceCells::Wedge:
        return VTKCellType::VTK_LAGRANGE_WEDGE;
      case ReferenceCells::Hexahedron:
        return VTKCellType::VTK_LAGRANGE_HEXAHEDRON;
      case ReferenceCells::Invalid:
        return VTKCellType::VTK_INVALID;
      default:
        Assert(false, ExcNotImplemented());
    }

  return VTKCellType::VTK_INVALID;
}



template <>
unsigned int
ReferenceCell::vtk_lexicographic_to_node_index<0>(
  const std::array<unsigned, 0> &,
  const std::array<unsigned, 0> &,
  const bool) const
{
  Assert(false, ExcNotImplemented());
  return 0;
}



template <>
unsigned int
ReferenceCell::vtk_lexicographic_to_node_index<1>(
  const std::array<unsigned, 1> &,
  const std::array<unsigned, 1> &,
  const bool) const
{
  Assert(false, ExcNotImplemented());
  return 0;
}



/**
 * Modified from
 * https://github.com/Kitware/VTK/blob/265ca48a/Common/DataModel/vtkLagrangeQuadrilateral.cxx#L558
 */
template <>
unsigned int
ReferenceCell::vtk_lexicographic_to_node_index<2>(
  const std::array<unsigned, 2> &node_indices,
  const std::array<unsigned, 2> &nodes_per_direction,
  const bool) const
{
  Assert(*this == ReferenceCells::Quadrilateral, ExcNotImplemented());

  const unsigned int i = node_indices[0];
  const unsigned int j = node_indices[1];

  const bool ibdy = (i == 0 || i == nodes_per_direction[0]);
  const bool jbdy = (j == 0 || j == nodes_per_direction[1]);
  // How many boundaries do we lie on at once?
  const int nbdy = (ibdy ? 1 : 0) + (jbdy ? 1 : 0);

  if (nbdy == 2) // Vertex DOF
    { // ijk is a corner node. Return the proper index (somewhere in [0,3]):
      return (i != 0u ? (j != 0u ? 2 : 1) : (j != 0u ? 3 : 0));
    }

  int offset = 4;
  if (nbdy == 1) // Edge DOF
    {
      if (!ibdy)
        { // On i axis
          return (i - 1) +
                 (j != 0u ?
                    nodes_per_direction[0] - 1 + nodes_per_direction[1] - 1 :
                    0) +
                 offset;
        }

      if (!jbdy)
        { // On j axis
          return (j - 1) +
                 (i != 0u ? nodes_per_direction[0] - 1 :
                            2 * (nodes_per_direction[0] - 1) +
                              nodes_per_direction[1] - 1) +
                 offset;
        }
    }

  offset += 2 * (nodes_per_direction[0] - 1 + nodes_per_direction[1] - 1);
  // nbdy == 0: Face DOF
  return offset + (i - 1) + (nodes_per_direction[0] - 1) * ((j - 1));
}



/**
 * Modified from
 * https://github.com/Kitware/VTK/blob/265ca48a/Common/DataModel/vtkLagrangeHexahedron.cxx#L734
 * (legacy_format=true) and from
 * https://github.com/Kitware/VTK/blob/256fe70de00e3441f126276ca4a8c5477d0bcb86/Common/DataModel/vtkHigherOrderHexahedron.cxx#L593
 * (legacy_format=false). The two versions differ regarding the ordering of
 * lines 10 and 11 (clockwise vs. anti-clockwise). See also:
 * https://github.com/Kitware/VTK/blob/7a0b92864c96680b1f42ee84920df556fc6ebaa3/Documentation/release/dev/node-numbering-change-for-VTK_LAGRANGE_HEXAHEDRON.md
 *
 */
template <>
unsigned int
ReferenceCell::vtk_lexicographic_to_node_index<3>(
  const std::array<unsigned, 3> &node_indices,
  const std::array<unsigned, 3> &nodes_per_direction,
  const bool                     legacy_format) const
{
  Assert(*this == ReferenceCells::Hexahedron, ExcNotImplemented());

  const unsigned int i = node_indices[0];
  const unsigned int j = node_indices[1];
  const unsigned int k = node_indices[2];

  const bool ibdy = (i == 0 || i == nodes_per_direction[0]);
  const bool jbdy = (j == 0 || j == nodes_per_direction[1]);
  const bool kbdy = (k == 0 || k == nodes_per_direction[2]);
  // How many boundaries do we lie on at once?
  const int nbdy = (ibdy ? 1 : 0) + (jbdy ? 1 : 0) + (kbdy ? 1 : 0);

  if (nbdy == 3) // Vertex DOF
    { // ijk is a corner node. Return the proper index (somewhere in [0,7]):
      return (i != 0u ? (j != 0u ? 2 : 1) : (j != 0u ? 3 : 0)) +
             (k != 0u ? 4 : 0);
    }

  int offset = 8;
  if (nbdy == 2) // Edge DOF
    {
      if (!ibdy)
        { // On i axis
          return (i - 1) +
                 (j != 0u ?
                    nodes_per_direction[0] - 1 + nodes_per_direction[1] - 1 :
                    0) +
                 (k != 0u ? 2 * (nodes_per_direction[0] - 1 +
                                 nodes_per_direction[1] - 1) :
                            0) +
                 offset;
        }
      if (!jbdy)
        { // On j axis
          return (j - 1) +
                 (i != 0u ? nodes_per_direction[0] - 1 :
                            2 * (nodes_per_direction[0] - 1) +
                              nodes_per_direction[1] - 1) +
                 (k != 0u ? 2 * (nodes_per_direction[0] - 1 +
                                 nodes_per_direction[1] - 1) :
                            0) +
                 offset;
        }
      // !kbdy, On k axis
      offset +=
        4 * (nodes_per_direction[0] - 1) + 4 * (nodes_per_direction[1] - 1);
      if (legacy_format)
        return (k - 1) +
               (nodes_per_direction[2] - 1) *
                 (i != 0u ? (j != 0u ? 3 : 1) : (j != 0u ? 2 : 0)) +
               offset;
      else
        return (k - 1) +
               (nodes_per_direction[2] - 1) *
                 (i != 0u ? (j != 0u ? 2 : 1) : (j != 0u ? 3 : 0)) +
               offset;
    }

  offset += 4 * (nodes_per_direction[0] - 1 + nodes_per_direction[1] - 1 +
                 nodes_per_direction[2] - 1);
  if (nbdy == 1) // Face DOF
    {
      if (ibdy) // On i-normal face
        {
          return (j - 1) + ((nodes_per_direction[1] - 1) * (k - 1)) +
                 (i != 0u ? (nodes_per_direction[1] - 1) *
                              (nodes_per_direction[2] - 1) :
                            0) +
                 offset;
        }
      offset += 2 * (nodes_per_direction[1] - 1) * (nodes_per_direction[2] - 1);
      if (jbdy) // On j-normal face
        {
          return (i - 1) + ((nodes_per_direction[0] - 1) * (k - 1)) +
                 (j != 0u ? (nodes_per_direction[2] - 1) *
                              (nodes_per_direction[0] - 1) :
                            0) +
                 offset;
        }
      offset += 2 * (nodes_per_direction[2] - 1) * (nodes_per_direction[0] - 1);
      // kbdy, On k-normal face
      return (i - 1) + ((nodes_per_direction[0] - 1) * (j - 1)) +
             (k != 0u ?
                (nodes_per_direction[0] - 1) * (nodes_per_direction[1] - 1) :
                0) +
             offset;
    }

  // nbdy == 0: Body DOF
  offset += 2 * ((nodes_per_direction[1] - 1) * (nodes_per_direction[2] - 1) +
                 (nodes_per_direction[2] - 1) * (nodes_per_direction[0] - 1) +
                 (nodes_per_direction[0] - 1) * (nodes_per_direction[1] - 1));
  return offset + (i - 1) +
         (nodes_per_direction[0] - 1) *
           ((j - 1) + (nodes_per_direction[1] - 1) * ((k - 1)));
}



unsigned int
ReferenceCell::vtk_vertex_to_deal_vertex(const unsigned int vertex_index) const
{
  AssertIndexRange(vertex_index, n_vertices());

  // For some of the following, deal.II uses the same ordering as VTK
  // and in that case, we only need to return 'vertex_index' (i.e.,
  // use the identity mapping). For some others, we need to translate.
  //
  // For the ordering, see the VTK manual (for example at
  // http://www.princeton.edu/~efeibush/viscourse/vtk.pdf, page 9).
  switch (this->kind)
    {
      case ReferenceCells::Vertex:
        return vertex_index;
      case ReferenceCells::Line:
        return vertex_index;
      case ReferenceCells::Triangle:
        return vertex_index;
      case ReferenceCells::Quadrilateral:
        {
          static constexpr std::array<unsigned int, 4> index_translation_table =
            {{0, 1, 3, 2}};
          return index_translation_table[vertex_index];
        }
      case ReferenceCells::Tetrahedron:
        return vertex_index;
      case ReferenceCells::Pyramid:
        {
          static constexpr std::array<unsigned int, 5> index_translation_table =
            {{0, 1, 3, 2, 4}};
          return index_translation_table[vertex_index];
        }
      case ReferenceCells::Wedge:
        return vertex_index;
      case ReferenceCells::Hexahedron:
        {
          static constexpr std::array<unsigned int, 8> index_translation_table =
            {{0, 1, 3, 2, 4, 5, 7, 6}};
          return index_translation_table[vertex_index];
        }
      case ReferenceCells::Invalid:
        {
          Assert(false, ExcNotImplemented());
          return numbers::invalid_unsigned_int;
        }
      default:
        Assert(false, ExcNotImplemented());
    }

  return numbers::invalid_unsigned_int;
}



unsigned int
ReferenceCell::gmsh_element_type() const
{
  /*
    From the GMSH documentation:

    elm-type
    defines the geometrical type of the n-th element:

    1
    Line (2 nodes).

    2
    Triangle (3 nodes).

    3
    Quadrangle (4 nodes).

    4
    Tetrahedron (4 nodes).

    5
    Hexahedron (8 nodes).

    6
    Prism (6 nodes).

    7
    Pyramid (5 nodes).

    8
    Second order line (3 nodes: 2 associated with the vertices and 1 with the
    edge).

    9
    Second order triangle (6 nodes: 3 associated with the vertices and 3 with
    the edges).

    10 Second order quadrangle (9 nodes: 4 associated with the
    vertices, 4 with the edges and 1 with the face).

    11 Second order tetrahedron (10 nodes: 4 associated with the vertices and 6
    with the edges).

    12 Second order hexahedron (27 nodes: 8 associated with the vertices, 12
    with the edges, 6 with the faces and 1 with the volume).

    13 Second order prism (18 nodes: 6 associated with the vertices, 9 with the
    edges and 3 with the quadrangular faces).

    14 Second order pyramid (14 nodes: 5 associated with the vertices, 8 with
    the edges and 1 with the quadrangular face).

    15 Point (1 node).
  */

  switch (this->kind)
    {
      case ReferenceCells::Vertex:
        return 15;
      case ReferenceCells::Line:
        return 1;
      case ReferenceCells::Triangle:
        return 2;
      case ReferenceCells::Quadrilateral:
        return 3;
      case ReferenceCells::Tetrahedron:
        return 4;
      case ReferenceCells::Pyramid:
        return 7;
      case ReferenceCells::Wedge:
        return 6;
      case ReferenceCells::Hexahedron:
        return 5;
      case ReferenceCells::Invalid:
      default:
        Assert(false, ExcNotImplemented());
    }

  return numbers::invalid_unsigned_int;
}



std::ostream &
operator<<(std::ostream &out, const ReferenceCell &reference_cell)
{
  AssertThrow(out.fail() == false, ExcIO());

  // Output as an integer to avoid outputting it as a character with
  // potentially non-printing value:
  out << static_cast<unsigned int>(reference_cell.kind);
  return out;
}



std::istream &
operator>>(std::istream &in, ReferenceCell &reference_cell)
{
  AssertThrow(in.fail() == false, ExcIO());

  // Read the information as an integer and convert it to the correct type
  unsigned int value;
  in >> value;
  reference_cell.kind = static_cast<decltype(reference_cell.kind)>(value);

  // Ensure that the object we read is valid
  Assert(
    (reference_cell == ReferenceCells::Vertex) ||
      (reference_cell == ReferenceCells::Line) ||
      (reference_cell == ReferenceCells::Triangle) ||
      (reference_cell == ReferenceCells::Quadrilateral) ||
      (reference_cell == ReferenceCells::Tetrahedron) ||
      (reference_cell == ReferenceCells::Hexahedron) ||
      (reference_cell == ReferenceCells::Wedge) ||
      (reference_cell == ReferenceCells::Pyramid) ||
      (reference_cell == ReferenceCells::Invalid),
    ExcMessage(
      "The reference cell kind just read does not correspond to one of the "
      "valid choices. There must be an error."));

  return in;
}



#include "reference_cell.inst"

DEAL_II_NAMESPACE_CLOSE
