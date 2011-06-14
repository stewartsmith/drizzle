/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 Sun Microsystems, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

namespace drizzled
{
namespace optimizer
{
class CostVector
{

public:
  CostVector() :
    io_count(0.0),
    avg_io_cost(1.0),
    cpu_cost(0.0),
    mem_cost(0.0),
    import_cost(0.0)
  {}

  double total_cost()
  {
    return IO_COEFF*io_count*avg_io_cost + CPU_COEFF * cpu_cost +
      MEM_COEFF*mem_cost + IMPORT_COEFF*import_cost;
  }

  void zero()
  {
    avg_io_cost= 1.0;
    io_count= cpu_cost= mem_cost= import_cost= 0.0;
  }

  void multiply(double m)
  {
    io_count *= m;
    cpu_cost *= m;
    import_cost *= m;
    /* Don't multiply mem_cost */
  }

  void add(const CostVector* cost)
  {
    double io_count_sum= io_count + cost->io_count;
    add_io(cost->io_count, cost->avg_io_cost);
    io_count= io_count_sum;
    cpu_cost += cost->cpu_cost;
  }
  void add_io(double add_io_cnt, double add_avg_cost)
  {
    double io_count_sum= io_count + add_io_cnt;
    avg_io_cost= (io_count * avg_io_cost +
                  add_io_cnt * add_avg_cost) / io_count_sum;
    io_count= io_count_sum;
  }

  /* accessor methods*/
  void setIOCount(double m)
  {
     io_count= m;
  }
  double getIOCount() const 
  {
     return io_count;
  }
  void setAvgIOCost(double m)
  {
     avg_io_cost= m;
  }
  double getAvgIOCost() const 
  {
     return avg_io_cost;
  }
  void setCpuCost(double m)
  { 
     cpu_cost= m;
  }
  double getCpuCost() const
  {
     return cpu_cost;
  }
  void setMemCost(double m)
  { 
     mem_cost= m;
  }
  double getMemCost() const
  {
     return mem_cost;
  }
  void setImportCost(double m)
  {
     import_cost= m;
  }
  double getImportCost() const
  {
     return import_cost;
  }

private:

  double io_count;     /* number of I/O                 */
  double avg_io_cost;  /* cost of an average I/O oper.  */
  double cpu_cost;     /* cost of operations in CPU     */
  double mem_cost;     /* cost of used memory           */
  double import_cost;  /* cost of remote operations     */

  static const uint32_t IO_COEFF=1;
  static const uint32_t CPU_COEFF=1;
  static const uint32_t MEM_COEFF=1;
  static const uint32_t IMPORT_COEFF=1;

};
} /* namespace optimizer */
} /* namespace drizzled */

