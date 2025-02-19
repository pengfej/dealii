// ---------------------------------------------------------------------
//
// Copyright (C) 2009 - 2020 by the deal.II authors
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



// this function tests the correctness of the implementation of
// inhomogeneous constraints on a nonsymmetric matrix that comes from a
// discretization of the first derivative, based on block matrices instead
// of standard matrices, by working on a vector-valued problem

#include <deal.II/base/function.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/utilities.h>

#include <deal.II/dofs/dof_accessor.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/fe/fe_values.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_refinement.h>
#include <deal.II/grid/tria.h>

#include <deal.II/lac/affine_constraints.h>
#include <deal.II/lac/block_sparse_matrix.h>
#include <deal.II/lac/block_sparsity_pattern.h>
#include <deal.II/lac/block_vector.h>
#include <deal.II/lac/full_matrix.h>

#include <deal.II/numerics/error_estimator.h>
#include <deal.II/numerics/matrix_tools.h>
#include <deal.II/numerics/vector_tools.h>

#include <complex>
#include <iostream>

#include "../tests.h"

template <int dim>
class AdvectionProblem
{
public:
  AdvectionProblem();
  ~AdvectionProblem();

  void
  run();

private:
  void
  setup_system();
  void
  test_equality();
  void
  assemble_reference();
  void
  assemble_test_1();
  void
  assemble_test_2();

  Triangulation<dim> triangulation;

  DoFHandler<dim> dof_handler;
  FESystem<dim>   fe;

  AffineConstraints<double> hanging_nodes_only;
  AffineConstraints<double> test_all_constraints;

  BlockSparsityPattern      sparsity_pattern;
  BlockSparseMatrix<double> reference_matrix;
  BlockSparseMatrix<double> test_matrix;

  BlockVector<double> reference_rhs;
  BlockVector<double> test_rhs;
};



template <int dim>
class RightHandSide : public Function<dim>
{
public:
  RightHandSide()
    : Function<dim>()
  {}

  virtual double
  value(const Point<dim> &p, const unsigned int component) const;
};


template <int dim>
double
RightHandSide<dim>::value(const Point<dim> &p,
                          const unsigned int /*component*/) const
{
  double product = 1;
  for (unsigned int d = 0; d < dim; ++d)
    product *= (p[d] + 1);
  return product;
}


template <int dim>
AdvectionProblem<dim>::AdvectionProblem()
  : dof_handler(triangulation)
  , fe(FE_Q<dim>(2), 2)
{}


template <int dim>
AdvectionProblem<dim>::~AdvectionProblem()
{
  dof_handler.clear();
}


template <int dim>
void
AdvectionProblem<dim>::setup_system()
{
  dof_handler.distribute_dofs(fe);

  hanging_nodes_only.clear();
  test_all_constraints.clear();

  // add boundary conditions as
  // inhomogeneous constraints here.
  {
    std::map<types::global_dof_index, double> boundary_values;
    VectorTools::interpolate_boundary_values(
      dof_handler, 0, Functions::ConstantFunction<dim>(1., 2), boundary_values);
    std::map<types::global_dof_index, double>::const_iterator boundary_value =
      boundary_values.begin();
    for (; boundary_value != boundary_values.end(); ++boundary_value)
      {
        test_all_constraints.add_line(boundary_value->first);
        test_all_constraints.set_inhomogeneity(boundary_value->first,
                                               boundary_value->second);
      }
  }
  DoFTools::make_hanging_node_constraints(dof_handler, hanging_nodes_only);
  DoFTools::make_hanging_node_constraints(dof_handler, test_all_constraints);
  hanging_nodes_only.close();
  test_all_constraints.close();

  BlockDynamicSparsityPattern csp(2, 2);
  {
    const unsigned int dofs_per_block = dof_handler.n_dofs() / 2;
    csp.block(0, 0).reinit(dofs_per_block, dofs_per_block);
    csp.block(0, 1).reinit(dofs_per_block, dofs_per_block);
    csp.block(1, 0).reinit(dofs_per_block, dofs_per_block);
    csp.block(1, 1).reinit(dofs_per_block, dofs_per_block);
    csp.collect_sizes();
  }

  DoFTools::make_sparsity_pattern(dof_handler, csp, hanging_nodes_only, true);
  sparsity_pattern.copy_from(csp);

  reference_matrix.reinit(sparsity_pattern);
  test_matrix.reinit(sparsity_pattern);

  reference_rhs.reinit(2);
  reference_rhs.block(0).reinit(dof_handler.n_dofs() / 2);
  reference_rhs.block(1).reinit(dof_handler.n_dofs() / 2);
  reference_rhs.collect_sizes();
  test_rhs.reinit(reference_rhs);
}



// test whether we are equal with the
// standard matrix and right hand side
template <int dim>
void
AdvectionProblem<dim>::test_equality()
{
  // need to manually go through the
  // matrix, since we can have different
  // entries in constrained lines because
  // the diagonal is set differently

  const BlockIndices &index_mapping = sparsity_pattern.get_column_indices();

  for (unsigned int i = 0; i < reference_matrix.m(); ++i)
    {
      const unsigned int block_row = index_mapping.global_to_local(i).first;
      const unsigned int index_in_block =
        index_mapping.global_to_local(i).second;
      for (unsigned int block_col = 0;
           block_col < sparsity_pattern.n_block_cols();
           ++block_col)
        {
          SparseMatrix<double>::const_iterator reference =
            reference_matrix.block(block_row, block_col).begin(index_in_block);
          SparseMatrix<double>::iterator test =
            test_matrix.block(block_row, block_col).begin(index_in_block);
          if (test_all_constraints.is_constrained(i) == false)
            {
              for (;
                   test !=
                   test_matrix.block(block_row, block_col).end(index_in_block);
                   ++test, ++reference)
                test->value() -= reference->value();
            }
          else
            for (; test !=
                   test_matrix.block(block_row, block_col).end(index_in_block);
                 ++test)
              test->value() = 0;
        }
    }

  double frobenius_norm = 0.;
  for (unsigned int row = 0; row < sparsity_pattern.n_block_rows(); ++row)
    for (unsigned int col = 0; col < sparsity_pattern.n_block_cols(); ++col)
      frobenius_norm += test_matrix.block(row, col).frobenius_norm() *
                        test_matrix.block(row, col).frobenius_norm();
  frobenius_norm = std::sqrt(frobenius_norm);

  deallog << "  Matrix difference norm: " << frobenius_norm << std::endl;
  Assert(frobenius_norm < 1e-13, ExcInternalError());

  // same here -- Dirichlet lines will have
  // nonzero rhs, whereas we will have zero
  // rhs when using inhomogeneous
  // constraints.
  for (unsigned int i = 0; i < reference_matrix.m(); ++i)
    if (test_all_constraints.is_constrained(i) == false)
      test_rhs(i) -= reference_rhs(i);
    else
      test_rhs(i) = 0;

  deallog << "  RHS difference norm: " << test_rhs.l2_norm() << std::endl;

  Assert(test_rhs.l2_norm() < 1e-14, ExcInternalError());
}



template <int dim>
void
AdvectionProblem<dim>::assemble_reference()
{
  reference_matrix = 0;
  reference_rhs    = 0;

  QGauss<dim>   quadrature_formula(3);
  FEValues<dim> fe_values(fe,
                          quadrature_formula,
                          update_values | update_gradients |
                            update_quadrature_points | update_JxW_values);

  const RightHandSide<dim> rhs_function;
  const unsigned int       dofs_per_cell = fe.dofs_per_cell;
  const unsigned int       n_q_points    = quadrature_formula.size();

  FullMatrix<double> cell_matrix(dofs_per_cell, dofs_per_cell);
  Vector<double>     cell_rhs(dofs_per_cell);

  std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);
  std::vector<double>                  rhs_values(n_q_points);

  typename DoFHandler<dim>::active_cell_iterator cell =
                                                   dof_handler.begin_active(),
                                                 endc = dof_handler.end();
  for (; cell != endc; ++cell)
    {
      cell_matrix = 0;
      cell_rhs    = 0;
      fe_values.reinit(cell);

      rhs_function.value_list(fe_values.get_quadrature_points(), rhs_values);

      Tensor<1, dim> advection_direction;
      advection_direction[0]       = 1;
      advection_direction[1]       = 1;
      advection_direction[dim - 1] = -1;

      for (unsigned int q_point = 0; q_point < n_q_points; ++q_point)
        for (unsigned int i = 0; i < dofs_per_cell; ++i)
          {
            const unsigned int comp_i = fe.system_to_component_index(i).first;
            for (unsigned int j = 0; j < dofs_per_cell; ++j)
              {
                const unsigned int comp_j =
                  fe.system_to_component_index(j).first;
                if (comp_i == comp_j)
                  cell_matrix(i, j) +=
                    (fe_values.shape_value(i, q_point) * advection_direction *
                     fe_values.shape_grad(j, q_point) * fe_values.JxW(q_point));
              }

            cell_rhs(i) += (fe_values.shape_value(i, q_point) *
                            rhs_values[q_point] * fe_values.JxW(q_point));
          }

      local_dof_indices.resize(dofs_per_cell);
      cell->get_dof_indices(local_dof_indices);

      reference_matrix.add(local_dof_indices, cell_matrix);
      for (unsigned int i = 0; i < dofs_per_cell; ++i)
        reference_rhs(local_dof_indices[i]) += cell_rhs(i);
    }

  hanging_nodes_only.condense(reference_matrix, reference_rhs);

  // use some other rhs vector as dummy for
  // application of Dirichlet conditions
  std::map<types::global_dof_index, double> boundary_values;
  VectorTools::interpolate_boundary_values(
    dof_handler, 0, Functions::ConstantFunction<dim>(1., 2), boundary_values);
  MatrixTools::apply_boundary_values(boundary_values,
                                     reference_matrix,
                                     test_rhs,
                                     reference_rhs);
}



template <int dim>
void
AdvectionProblem<dim>::assemble_test_1()
{
  test_matrix = 0;
  test_rhs    = 0;


  QGauss<dim>   quadrature_formula(3);
  FEValues<dim> fe_values(fe,
                          quadrature_formula,
                          update_values | update_gradients |
                            update_quadrature_points | update_JxW_values);

  const RightHandSide<dim> rhs_function;
  const unsigned int       dofs_per_cell = fe.dofs_per_cell;
  const unsigned int       n_q_points    = quadrature_formula.size();

  FullMatrix<double> cell_matrix(dofs_per_cell, dofs_per_cell);
  Vector<double>     cell_rhs(dofs_per_cell);

  std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);
  std::vector<double>                  rhs_values(n_q_points);

  typename DoFHandler<dim>::active_cell_iterator cell =
                                                   dof_handler.begin_active(),
                                                 endc = dof_handler.end();
  for (; cell != endc; ++cell)
    {
      cell_matrix = 0;
      cell_rhs    = 0;
      fe_values.reinit(cell);

      rhs_function.value_list(fe_values.get_quadrature_points(), rhs_values);

      Tensor<1, dim> advection_direction;
      advection_direction[0]       = 1;
      advection_direction[1]       = 1;
      advection_direction[dim - 1] = -1;

      for (unsigned int q_point = 0; q_point < n_q_points; ++q_point)
        for (unsigned int i = 0; i < dofs_per_cell; ++i)
          {
            const unsigned int comp_i = fe.system_to_component_index(i).first;
            for (unsigned int j = 0; j < dofs_per_cell; ++j)
              {
                const unsigned int comp_j =
                  fe.system_to_component_index(j).first;
                if (comp_i == comp_j)
                  cell_matrix(i, j) +=
                    (fe_values.shape_value(i, q_point) * advection_direction *
                     fe_values.shape_grad(j, q_point) * fe_values.JxW(q_point));
              }

            cell_rhs(i) += (fe_values.shape_value(i, q_point) *
                            rhs_values[q_point] * fe_values.JxW(q_point));
          }

      local_dof_indices.resize(dofs_per_cell);
      cell->get_dof_indices(local_dof_indices);

      test_matrix.add(local_dof_indices, cell_matrix);
      for (unsigned int i = 0; i < dofs_per_cell; ++i)
        test_rhs(local_dof_indices[i]) += cell_rhs(i);
    }

  test_all_constraints.condense(test_matrix, test_rhs);

  test_equality();
}



template <int dim>
void
AdvectionProblem<dim>::assemble_test_2()
{
  test_matrix = 0;
  test_rhs    = 0;

  QGauss<dim>   quadrature_formula(3);
  FEValues<dim> fe_values(fe,
                          quadrature_formula,
                          update_values | update_gradients |
                            update_quadrature_points | update_JxW_values);

  const RightHandSide<dim> rhs_function;
  const unsigned int       dofs_per_cell = fe.dofs_per_cell;
  const unsigned int       n_q_points    = quadrature_formula.size();

  FullMatrix<double> cell_matrix(dofs_per_cell, dofs_per_cell);
  Vector<double>     cell_rhs(dofs_per_cell);

  std::vector<types::global_dof_index> local_dof_indices(dofs_per_cell);
  std::vector<double>                  rhs_values(n_q_points);

  typename DoFHandler<dim>::active_cell_iterator cell =
                                                   dof_handler.begin_active(),
                                                 endc = dof_handler.end();
  for (; cell != endc; ++cell)
    {
      cell_matrix = 0;
      cell_rhs    = 0;
      fe_values.reinit(cell);

      rhs_function.value_list(fe_values.get_quadrature_points(), rhs_values);

      Tensor<1, dim> advection_direction;
      advection_direction[0]       = 1;
      advection_direction[1]       = 1;
      advection_direction[dim - 1] = -1;

      for (unsigned int q_point = 0; q_point < n_q_points; ++q_point)
        for (unsigned int i = 0; i < dofs_per_cell; ++i)
          {
            const unsigned int comp_i = fe.system_to_component_index(i).first;
            for (unsigned int j = 0; j < dofs_per_cell; ++j)
              {
                const unsigned int comp_j =
                  fe.system_to_component_index(j).first;
                if (comp_i == comp_j)
                  cell_matrix(i, j) +=
                    (fe_values.shape_value(i, q_point) * advection_direction *
                     fe_values.shape_grad(j, q_point) * fe_values.JxW(q_point));
              }

            cell_rhs(i) += (fe_values.shape_value(i, q_point) *
                            rhs_values[q_point] * fe_values.JxW(q_point));
          }

      local_dof_indices.resize(dofs_per_cell);
      cell->get_dof_indices(local_dof_indices);

      test_all_constraints.distribute_local_to_global(
        cell_matrix, cell_rhs, local_dof_indices, test_matrix, test_rhs);
    }
  test_equality();
}


template <int dim>
void
AdvectionProblem<dim>::run()
{
  GridGenerator::hyper_cube(triangulation);
  triangulation.refine_global(4 - dim);

  // manually refine the first two cells to
  // create some hanging nodes
  {
    typename DoFHandler<dim>::active_cell_iterator cell =
      dof_handler.begin_active();
    cell->set_refine_flag();
  }
  triangulation.execute_coarsening_and_refinement();
  {
    // find the last cell and mark it
    // for refinement
    for (typename DoFHandler<dim>::active_cell_iterator cell =
           dof_handler.begin_active();
         cell != dof_handler.end();
         ++cell)
      if (++typename DoFHandler<dim>::active_cell_iterator(cell) ==
          dof_handler.end())
        cell->set_refine_flag();
  }
  triangulation.execute_coarsening_and_refinement();

  setup_system();

  deallog << std::endl
          << std::endl
          << "  Number of active cells:       "
          << triangulation.n_active_cells() << std::endl
          << "  Number of degrees of freedom: " << dof_handler.n_dofs()
          << std::endl
          << "  Number of constraints       : "
          << hanging_nodes_only.n_constraints() << std::endl;

  assemble_reference();
  assemble_test_1();
  assemble_test_2();
}



int
main()
{
  initlog();
  deallog << std::setprecision(2);
  deallog.get_file_stream() << std::setprecision(2);

  {
    AdvectionProblem<2> advection_problem;
    advection_problem.run();
  }
  {
    AdvectionProblem<3> advection_problem;
    advection_problem.run();
  }
}
