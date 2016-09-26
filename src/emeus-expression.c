/* emeus-expression-private.c: A set of terms and a constant
 *
 * Copyright 2016  Endless
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "emeus-expression-private.h"
#include "emeus-variable-private.h"
#include "emeus-simplex-solver-private.h"

#include <glib.h>
#include <math.h>
#include <float.h>

static Term *
term_new (Variable *variable,
          double coefficient)
{
  Term *t = g_slice_new (Term);

  t->variable = variable_ref (variable);
  t->coefficient = coefficient;

  return t;
}

static void
term_free (Term *term)
{
  if (term == NULL)
    return;

  variable_unref (term->variable);

  g_slice_free (Term, term);
}

static Expression *
expression_new_full (SimplexSolver *solver,
                     Variable *variable,
                     double coefficient,
                     double constant)
{
  Expression *res = g_slice_new (Expression);

  res->solver = solver;
  res->constant = constant;
  res->terms = NULL;
  res->ref_count = 1;

  if (variable != NULL)
    expression_add_variable (res, variable, coefficient);

  return res;
}

Expression *
expression_new (SimplexSolver *solver,
                double constant)
{
  return expression_new_full (solver, NULL, 0.0, constant);
}

Expression *
expression_new_from_variable (Variable *variable)
{
  return expression_new_full (variable->solver, variable, 1.0, 0.0);
}

static void
expression_add_term (Expression *expression,
                     Term *term)
{
  if (expression->terms == NULL)
    expression->terms = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) term_free);

  g_hash_table_insert (expression->terms, term->variable, term);
}

Expression *
expression_clone (Expression *expression)
{
  Expression *clone = expression_new_full (expression->solver,
                                           NULL, 0.0,
                                           expression->constant);
  GHashTableIter iter;
  gpointer value_p;

  if (expression->terms == NULL)
    return clone;

  g_hash_table_iter_init (&iter, expression->terms);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Term *t = value_p;

      expression_add_term (clone, term_new (t->variable, t->coefficient));
    }

  return clone;
}

Expression *
expression_ref (Expression *expression)
{
  if (expression == NULL)
    return NULL;

  expression->ref_count += 1;

  return expression;
}

void
expression_unref (Expression *expression)
{
  if (expression == NULL)
    return;

  expression->ref_count -= 1;

  if (expression->ref_count == 0)
    {
      if (expression->terms != NULL)
        g_hash_table_unref (expression->terms);

      g_slice_free (Expression, expression);
    }
}

void
expression_set_constant (Expression *expression,
                         double constant)
{
  expression->constant = constant;
}

void
expression_add_variable_with_subject (Expression *expression,
                                      Variable *variable,
                                      double coefficient,
                                      Variable *subject)
{
  if (expression->terms != NULL)
    {
      Term *t = g_hash_table_lookup (expression->terms, variable);

      if (t != NULL)
        {
          if (coefficient == 0.0)
            {
              if (subject != NULL)
                simplex_solver_remove_variable (expression->solver, t->variable, subject);

              g_hash_table_remove (expression->terms, t);
            }
          else
            t->coefficient = coefficient;

          return;
        }
    }

  expression_add_term (expression, term_new (variable, coefficient));

  if (subject != NULL)
    simplex_solver_add_variable (expression->solver, variable, subject);
}

static void
expression_remove_variable_with_subject (Expression *expression,
                                         Variable *variable,
                                         Variable *subject)
{
  if (expression->terms == NULL)
    return;

  simplex_solver_remove_variable (expression->solver, variable, subject);
  g_hash_table_remove (expression->terms, variable);
}

void
expression_set_variable (Expression *expression,
                         Variable *variable,
                         double coefficient)
{
  expression_add_term (expression, term_new (variable, coefficient));

  if (expression->solver != NULL && variable_is_external (variable))
    simplex_solver_update_variable (expression->solver, variable);
}

void
expression_add_expression (Expression *a,
                           Expression *b,
                           double n,
                           Variable *subject)
{
  GHashTableIter iter;
  gpointer value_p;

  a->constant += (n * b->constant);

  if (b->terms == NULL)
    return;

  g_hash_table_iter_init (&iter, b->terms);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Term *t = value_p;

      expression_add_variable_with_subject (a, t->variable, n * t->coefficient, subject);
    }
}

void
expression_add_variable (Expression *expression,
                         Variable *variable,
                         double coefficient)
{
  expression_add_variable_with_subject (expression, variable, coefficient, NULL);
}

void
expression_remove_variable (Expression *expression,
                            Variable *variable)
{
  expression_remove_variable_with_subject (expression, variable, NULL);
}

void
expression_set_coefficient (Expression *expression,
                            Variable *variable,
                            double coefficient)
{
  if (coefficient == 0.0)
    expression_remove_variable (expression, variable);
  else
    {
      Term *t = g_hash_table_lookup (expression->terms, variable);

      if (t != NULL)
        {
          t->coefficient = coefficient;

          if (variable_is_external (t->variable))
            simplex_solver_update_variable (expression->solver, t->variable);
        }
      else
        expression_add_variable (expression, variable, coefficient);
    }
}

double
expression_get_coefficient (const Expression *expression,
                            Variable *variable)
{
  Term *t;

  if (expression->terms == NULL)
    return 0.0;

  t = g_hash_table_lookup (expression->terms, variable);
  if (t == NULL)
    return 0.0;

  return term_get_coefficient (t);
}

double
expression_get_value (const Expression *expression)
{
  GHashTableIter iter;
  gpointer value_p;
  double res;

  res = expression->constant;

  if (expression->terms == NULL)
    return res;

  g_hash_table_iter_init (&iter, expression->terms);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      const Term *t = value_p;

      res += term_get_value (t);
    }

  return res;
}

void
expression_terms_foreach (Expression *expression,
                          ExpressionForeachTermFunc func,
                          gpointer data)
{
  GHashTableIter iter;
  gpointer key_p, value_p;

  if (expression->terms == NULL)
    return;

  g_hash_table_iter_init (&iter, expression->terms);
  while (g_hash_table_iter_next (&iter, &key_p, &value_p))
    {
      Variable *v = key_p;
      Term *t = value_p;

      g_assert (v == t->variable);

      func (t, data);
    }
}

Expression *
expression_plus (Expression *expression,
                 double constant)
{
  Expression *e = expression_new (expression->solver, constant);

  expression_add_expression (expression, e, 1.0, NULL);

  expression_unref (e);

  return expression;
}

Expression *
expression_plus_variable (Expression *expression,
                          Variable *variable)
{
  Expression *e = expression_new_from_variable (variable);

  expression_add_expression (expression, e, 1.0, NULL);

  expression_unref (e);

  return expression;
}

Expression *
expression_times (Expression *expression,
                  double multiplier)
{
  GHashTableIter iter;
  gpointer value_p;

  expression->constant *= multiplier;

  if (expression->terms == NULL)
    return expression;

  g_hash_table_iter_init (&iter, expression->terms);
  while (g_hash_table_iter_next (&iter, NULL, &value_p))
    {
      Term *t = value_p;

      t->coefficient *= multiplier;
    }

  return expression;
}

void
expression_change_subject (Expression *expression,
                           Variable *old_subject,
                           Variable *new_subject)
{
  expression_set_variable (expression,
                           old_subject,
                           expression_new_subject (expression, new_subject));
}

double
expression_new_subject (Expression *expression,
                        Variable *subject)
{
  double reciprocal;
  Term *term;

  term = g_hash_table_lookup (expression->terms, subject);

  reciprocal = 0.0;
  if (fabs (term_get_value (term)) > DBL_EPSILON)
    reciprocal = 1.0 / term_get_value (term);

  g_hash_table_remove (expression->terms, subject);

  expression_times (expression, -1.0 * reciprocal);

  return reciprocal;
}

Variable *
expression_get_pivotable_variable (Expression *expression)
{
  GHashTableIter iter;
  gpointer key_p;

  if (expression->terms == NULL)
    {
      g_critical ("Expression %p is a constant", expression);
      return NULL;
    }

  g_hash_table_iter_init (&iter, expression->terms);
  while (g_hash_table_iter_next (&iter, &key_p, NULL))
    {
      if (variable_is_pivotable (key_p))
        return key_p;
    }

  return NULL;
}
