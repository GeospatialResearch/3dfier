/*
  3dfier: takes 2D GIS datasets and "3dfies" to create 3D city models.

  Copyright (C) 2015-2018  3D geoinformation research group, TU Delft

  This file is part of 3dfier.

  3dfier is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  3dfier is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with 3difer.  If not, see <http://www.gnu.org/licenses/>.

  For any information or further details about the use of 3dfier, contact
  Hugo Ledoux
  <h.ledoux@tudelft.nl>
  Faculty of Architecture & the Built Environment
  Delft University of Technology
  Julianalaan 134, Delft 2628BL, the Netherlands
*/

#include "geomtools.h"
#include "io.h"
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Projection_traits_xy_3.h>
#include <CGAL/Triangulation_vertex_base_with_id_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Polygon_2.h>

#include <vector>
#include <unordered_set>
#include <boost/heap/fibonacci_heap.hpp>

// fibonacci heap for greedy insertion code
struct point_error {
  point_error(int i, double e) : index(i), error(e){}
  int index;
  double error;
  
  bool operator<(point_error const & rhs) const
  {
    return error < rhs.error;
  }
};
typedef boost::heap::fibonacci_heap<point_error> Heap;
typedef Heap::handle_type heap_handle;

typedef CGAL::Exact_predicates_inexact_constructions_kernel			K;
typedef CGAL::Projection_traits_xy_3<K>								Gt;
typedef CGAL::Triangulation_vertex_base_with_id_2<Gt>				Vb;
struct FaceInfo2
{
  FaceInfo2() {}
  int nesting_level;
  bool in_domain() {
    return nesting_level % 2 == 1;
  }
  std::vector<heap_handle>* points_inside = nullptr;
  CGAL::Plane_3<K>* plane = nullptr;
};
typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo2, Gt>	Fbb;
typedef CGAL::Constrained_triangulation_face_base_2<Gt, Fbb>		Fb;
typedef CGAL::Triangulation_data_structure_2<Vb, Fb>				Tds;
typedef CGAL::Exact_predicates_tag									Itag;
typedef CGAL::Constrained_Delaunay_triangulation_2<Gt, Tds, Itag>	CDT;
typedef CDT::Point													Point;
typedef CGAL::Polygon_2<Gt>											Polygon_2;

struct PointXYHash {
  std::size_t operator()(Point const& p) const noexcept {
    std::size_t h1 = std::hash<double>{}(p.x());
    std::size_t h2 = std::hash<double>{}(p.y());
    return h1 ^ (h2 << 1);
  }
};
struct PointXYEqual {
  bool operator()(Point const& p1, Point const& p2) const noexcept {
    auto ex = p1.x() == p2.x();
    auto ey = p1.y() == p2.y();
    return ex && ey;
  }
};

inline double compute_error(Point &p, CDT::Face_handle &face);
void greedy_insert(CDT &T, const std::vector<Point3> &pts, double threshold);

void mark_domains(CDT& ct,
  CDT::Face_handle start,
  int index,
  std::list<CDT::Edge>& border) {
  if (start->info().nesting_level != -1) {
    return;
  }
  std::list<CDT::Face_handle> queue;
  queue.push_back(start);
  while (!queue.empty()) {
    CDT::Face_handle fh = queue.front();
    queue.pop_front();
    if (fh->info().nesting_level == -1) {
      fh->info().nesting_level = index;
      for (int i = 0; i < 3; i++) {
        CDT::Edge e(fh, i);
        CDT::Face_handle n = fh->neighbor(i);
        if (n->info().nesting_level == -1) {
          if (ct.is_constrained(e)) border.push_back(e);
          else queue.push_back(n);
        }
      }
    }
  }
}

//explore set of facets connected with non constrained edges,
//and attribute to each such set a nesting level.
//We start from facets incident to the infinite vertex, with a nesting
//level of 0. Then we recursively consider the non-explored facets incident 
//to constrained edges bounding the former set and increase the nesting level by 1.
//Facets in the domain are those with an odd nesting level.
void mark_domains(CDT& cdt) {
  for (CDT::All_faces_iterator it = cdt.all_faces_begin(); it != cdt.all_faces_end(); ++it) {
    it->info().nesting_level = -1;
  }
  std::list<CDT::Edge> border;
  mark_domains(cdt, cdt.infinite_face(), 0, border);
  while (!border.empty()) {
    CDT::Edge e = border.front();
    border.pop_front();
    CDT::Face_handle n = e.first->neighbor(e.second);
    if (n->info().nesting_level == -1) {
      mark_domains(cdt, n, e.first->info().nesting_level + 1, border);
    }
  }
}

bool getCDT(Polygon2* pgn,
  const std::vector< std::vector<int> > &z,
  std::vector< std::pair<Point3, std::string> > &vertices,
  std::vector<Triangle> &triangles,
  const std::vector<Point3> &lidarpts,
  double tinsimp_threshold) {
  CDT cdt;

  //-- gather all rings
  std::vector<Ring2> rings;
  rings.push_back(pgn->outer());
  for (Ring2& iring : pgn->inners())
    rings.push_back(iring);

  Polygon_2 poly;
  int ringi = -1;
  for (Ring2& ring : rings) {
    ringi++;
    for (int i = 0; i < ring.size(); i++) {
      poly.push_back(Point(bg::get<0>(ring[i]), bg::get<1>(ring[i]), z_to_float(z[ringi][i])));
    }
    cdt.insert_constraint(poly.vertices_begin(), poly.vertices_end(), true);
    poly.clear();
  }

  //-- add the lidar points to the CDT, if any
  if (lidarpts.size() > 0) {
    if (tinsimp_threshold != 0)
      greedy_insert(cdt, lidarpts, tinsimp_threshold);
    else {
      for (auto &pt : lidarpts) {
        cdt.insert(Point(bg::get<0>(pt), bg::get<1>(pt), bg::get<2>(pt)));
      }
    }
  }

  //Mark facets that are inside the domain bounded by the polygon
  mark_domains(cdt);

  unsigned index = 0;
  int count = 0;

  if (!cdt.is_valid()) {
    throw std::runtime_error("CDT is invalid.");
  }
  for (CDT::Finite_vertices_iterator vit = cdt.finite_vertices_begin();
    vit != cdt.finite_vertices_end(); ++vit) {
    Point3 p = Point3(vit->point().x(), vit->point().y(), vit->point().z());
    vertices.push_back(std::make_pair(p, gen_key_bucket(&p)));
    vit->id() = index++;
  }

  for (CDT::Finite_faces_iterator fit = cdt.finite_faces_begin();
    fit != cdt.finite_faces_end(); ++fit) {
    if (fit->info().in_domain()) {
      Triangle t;
      t.v0 = fit->vertex(0)->id();
      t.v1 = fit->vertex(1)->id();
      t.v2 = fit->vertex(2)->id();
      triangles.push_back(t);
      count++;
    }
  }
  return true;
}

std::string gen_key_bucket(const Point2* p) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3) << p->get<0>() << " " << p->get<1>();
  return ss.str();
}

std::string gen_key_bucket(const Point3* p) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3) << p->get<0>() << " " << p->get<1>() << " " << std::setprecision(2) << p->get<2>();
  return ss.str();
}

std::string gen_key_bucket(const Point3* p, float z) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(3) << p->get<0>() << " " << p->get<1>() << " " << std::setprecision(2) << z;
  return ss.str();
}

double distance(const Point2 &p1, const Point2 &p2) {
  double dx = p1.x() - p2.x();
  double dy = p1.y() - p2.y();
  return sqrt(dx * dx + dy * dy);
}

double sqr_distance(const Point2 &p1, const Point2 &p2) {
  double dx = p1.x() - p2.x();
  double dy = p1.y() - p2.y();
  return dx * dx + dy * dy;
}

//--- TIN Simplification
// Greedy insertion/incremental refinement algorithm adapted from "Fast polygonal approximation of terrain and height fields" by Garland, Michael and Heckbert, Paul S.
inline double compute_error(Point &p, CDT::Face_handle &face) {
  if(!face->info().plane)
    face->info().plane = new CGAL::Plane_3<K>(
      face->vertex(0)->point(),
      face->vertex(1)->point(),
      face->vertex(2)->point());
  if(!face->info().points_inside)
    face->info().points_inside = new std::vector<heap_handle>();

  auto plane = face->info().plane;
  auto interpolate = - plane->a()/plane->c() * p.x() - plane->b()/plane->c()*p.y() - plane->d()/plane->c();
  double error = std::fabs(interpolate - p.z());
  return error;
}

void greedy_insert(CDT &T, const std::vector<Point3> &pts, double threshold) {
  // assumes all lidar points are inside a triangle
  Heap heap;

  // Convert all elevation points to CGAL points
  std::vector<Point> cpts;
  cpts.reserve(pts.size());
  for (auto& p : pts) {
    cpts.push_back(Point(bg::get<0>(p), bg::get<1>(p), bg::get<2>(p)));
  }

  // compute initial point errors, build heap, store point indices in triangles
  {
    std::unordered_set<Point, PointXYHash, PointXYEqual> set;
    for(int i=0; i<cpts.size(); i++){
      auto p = cpts[i];
      // detect and skip duplicate points
      auto not_duplicate = set.insert(p).second;
      if(not_duplicate){
        auto face = T.locate(p);
        auto e = compute_error(p, face);
        auto handle = heap.push(point_error(i,e));
        face->info().points_inside->push_back(handle);
      }
    }
  }
  
  // insert points, update errors of affected triangles until threshold error is reached
  while (!heap.empty() && heap.top().error > threshold){
    // get top element (with largest error) from heap
    auto maxelement = heap.top();
    auto max_p = cpts[maxelement.index];

    // get triangles that will change after inserting this max_p
    std::vector<CDT::Face_handle> faces;
    T.get_conflicts ( max_p, std::back_inserter(faces) );

    // insert max_p in triangulation
    auto face_hint = faces[0];
    auto v = T.insert(max_p, face_hint);
    face_hint = v->face();
    
    // update clear info of triangles that just changed, collect points that were inside these triangles
    std::vector<heap_handle> points_to_update;
    for (auto face : faces) {
      if (face->info().plane){
        delete face->info().plane;
        face->info().plane = nullptr;
      }
      if (face->info().points_inside) {
        for (auto h :*face->info().points_inside){
          if( maxelement.index != (*h).index)
            points_to_update.push_back(h);
        }
        face->info().points_inside->clear();
      }
    }
    
    // remove the point we just inserted in the triangulation from the heap
    heap.pop();

    // update the errors of affected elevation points
    for (auto curelement : points_to_update){
      auto p = cpts[(*curelement).index];
      auto containing_face = T.locate(p, face_hint);
      const double e = compute_error(p, containing_face);
      const point_error new_pe = point_error((*curelement).index, e);
      heap.update(curelement, new_pe);
      containing_face->info().points_inside->push_back(curelement);
    }
  }

  //cleanup the stuff I put in face info of triangles
  for (CDT::Finite_faces_iterator fit = T.finite_faces_begin();
    fit != T.finite_faces_end(); ++fit) {
      if (fit->info().plane){
        delete fit->info().plane;
        fit->info().plane = nullptr;
      }
      if (fit->info().points_inside) {
        delete fit->info().points_inside;
        fit->info().points_inside = nullptr;
      }
    }
}

//--- Point-in-polygon grid
// Implementation of the grid center point algorithm by Li and Wang 2013
int sgn(double val) {
  return (double(0) < val) - (val < double(0));
};

void Grid::prepare() {
  //-- Calculate Bbox
  Box2 bbox = bg::return_envelope<Box2>(*polygon);
  this->xmin = bg::get<bg::min_corner, 0>(bbox);
  this->xmax = bg::get<bg::max_corner, 0>(bbox);
  this->ymin = bg::get<bg::min_corner, 1>(bbox);
  this->ymax = bg::get<bg::max_corner, 1>(bbox);
  
  // add 1 percent to bbox to overcome finite arithmetics
  double a = 0.01 * (this->xmax - this->xmin);
  double b = 0.01 * (this->ymax - this->ymin);
  this->xmin -= a;
  this->xmax += a;
  this->ymin -= b;
  this->ymax += b;
  
  //-- Calculate cell size
  double deltax = (xmax - xmin);
  double deltay = (ymax - ymin);
  double ration = deltax / deltay;

  double sqrtnr = std::sqrt(totcells);
  cellsx = 2 * (1 + (int)(ration * sqrtnr));
  if (cellsx > celllimit) cellsx = celllimit;
  sizex = (xmax - xmin) / cellsx;

  cellsy = 2 * (1 + (int)(sqrtnr / ration));
  if (cellsy > celllimit) cellsy = celllimit;
  sizey = (ymax - ymin) / cellsy;

  //-- Create empty list of GridCells
  int i, j;
  cells = new GridCell**[cellsx];
  for (i = 0; i < cellsx; i++)
    cells[i] = new GridCell*[cellsy];

  for (i = 0; i < cellsx; i++)
    for (j = 0; j < cellsy; j++)
      cells[i][j] = new GridCell();

  constructEdges();
  rasterize();
  markCells();
}

void Grid::constructEdges() {
  for (int ringi = 0; ringi <= polygon->inners().size(); ringi++) {
    Ring2 r;
    if (ringi == 0) {
      r = polygon->outer();
    }
    else {
      r = polygon->inners()[ringi - 1];
    }
  
    int n = r.size() - 1;
    for (int i = 1; i <= n; i++) {
      this->edges.push_back(new PolyEdge(r[i - 1].x(), r[i - 1].y(), r[i].x(), r[i].y()));
    }
    this->edges.push_back(new PolyEdge(r[n].x(), r[n].y(), r[0].x(), r[0].y()));
  }
};

void Grid::rasterize(){
  for (PolyEdge* e : edges) {
    // Get direction vector
    double xlen = e->x2 - e->x1;
    double ylen = e->y2 - e->y1;

    // Normalize vector
    double normal = std::sqrt(xlen * xlen + ylen * ylen);
    double dirx = xlen / normal;
    double diry = ylen / normal;

    // Get direction sign
    int stepx = sgn(dirx);
    int stepy = sgn(diry);

    // Get start cell grid location
    int xp = (int)((e->x1 - xmin) / sizex);
    int yp = (int)((e->y1 - ymin) / sizey);
    // Add edge to start cell
    cells[xp][yp]->edges.push_back(e);

    // Get end cell grid location
    int lastxp = (int)((e->x2 - xmin) / sizex);
    int lastyp = (int)((e->y2 - ymin) / sizey);

    // Calculate length to next grid edge crossing
    double deltax, deltay;
    if (stepx == 0) {
      deltax = DBL_MAX;
    }
    else {
      deltax = sizex / std::abs(dirx);
    }

    if (stepy == 0) {
      deltay = DBL_MAX;
    }
    else {
      deltay = sizey / std::abs(diry);
    }

    double k = e->x1 - (xp * sizex + xmin);
    double dx = k * (deltax / sizex);
    if (stepx != -1) {
      dx = deltax - dx;
    }

    k = e->y1 - (yp * sizey + ymin);
    double dy = k * (deltay / sizey);
    if (stepy != -1) {
      dy = deltay - dy;
    }

    // Loop through cells untill the end of edge is reached
    while (lastxp != xp || lastyp != yp) {
      if (dx <= dy) {
        dx += deltax;
        xp += stepx;
      }
      if (dy <= dx) {
        dy += deltay;
        yp += stepy;
      }
      cells[xp][yp]->edges.push_back(e);
    }
  }
}

int HorizontalRayLineIntersection(PolyEdge r, PolyEdge* e) {
  // line is completely above or below ray (only horizontal rays are allowed)
  if ((e->y1 < r.y1 && e->y2 < r.y1) || (e->y1 > r.y1 && e->y2 > r.y1)) {
    return 0;
  }
  double a1 = r.y2 - r.y1;
  double b1 = r.x2 - r.x1;
  double c1 = a1 * (r.x1) + b1 * (r.y1);

  double a2 = e->y2 - e->y1;
  double b2 = e->x2 - e->x1;
  double c2 = a1 * (e->x1) + b1 * (e->y1);

  double determinant = a1 * b2 - a2 * b1;

  if (determinant == 0) {
    // The lines are parallel. 
    return 0;
  }
  else {
    double x = (b2*c1 - b1*c2) / determinant;
    double y = (a1*c2 - a2*c1) / determinant;
    // Check if the ray end (center of cell) is on intersection, in that case cell is GRAY
    if (x == r.x2 && y == r.y2) {
      return -1;
    }
    return 1;
  }
}

bool RayLineIntersects(PolyEdge r, PolyEdge* e) {
  double a1 = r.y2 - r.y1;
  double b1 = r.x2 - r.x1;
  double c1 = a1 * (r.x1) + b1 * (r.y1);

  double a2 = e->y2 - e->y1;
  double b2 = e->x2 - e->x1;
  double c2 = a1 * (e->x1) + b1 * (e->y1);

  double determinant = a1 * b2 - a2 * b1;

  if (determinant == 0) {
    // The lines are parallel.
    return false;
  }
  else {
    return true;
  }
}

void Grid::markCells() {
  //-- Calculate value of GridCells center point
  int prevcolor = CBLACK;
  // start at x = -1/2 cell and y = 1/2 cell
  double x = xmin - (sizex / 2);
  double y = ymin + (sizey / 2);
  for (int iy = 0; iy < cellsy; iy++) {
    for (int ix = 0; ix < cellsx; ix++) {
      // Reset prev color to black for each new row
      if (ix = 0) {
        prevcolor = CBLACK;
        x = xmin + (sizex * (ix-0.5));
      }

      // store previous x,y
      double prevx = x;
      double prevy = y;

      // increase x-coordinate of cell
      x += sizex;

      //raytracing from center to center. Use edges from this and previous cell if not boundary
      int crossings = 0;
      int color = CBLACK;
      PolyEdge r = PolyEdge(prevx, prevy, x, y);
      // Iterate edges of current cell
      for (PolyEdge* e : cells[ix][iy]->edges) {
        int cross = HorizontalRayLineIntersection(r, e);
        if (cross == -1) {
          color = CSINGULAR;
          break;
        }
        crossings += cross;
      }
      // Iterate edges of previous cell
      if (color != CSINGULAR && ix > 0) {
        for (PolyEdge* e : cells[ix - 1][iy]->edges) {
          int cross = HorizontalRayLineIntersection(r, e);
          if (cross == -1) {
            color = CSINGULAR;
            break;
          }
          crossings += cross;
        }
      }
      if (color == CSINGULAR) {
        // in singular inverse previous color
        if (prevcolor == CBLACK) {
          prevcolor = CWHITE;
        }
        else {
          prevcolor = CBLACK;
        }
      }
      else
      {
        // equal crossings gives same color for cell
        if (crossings % 2 == 0) {
          color = prevcolor;
        }
        else {
          // inverse color
          if (prevcolor == CBLACK) {
            color = CWHITE;
          }
          else {
            color = CBLACK;
          }
        }
        prevcolor = color;
      }
      // Store color
      cells[ix][iy]->color = color;
    }
    // increase y-coordinate of cell
    y += sizey;
  }
}

PolyEdge Grid::getEdgeToCenter(int i, int j, double x, double y) {
  double xc = xmin + (sizex * (i - 0.5));
  double yc = ymin + (sizey * (i - 0.5));
  return PolyEdge(x, y, xc, yc);
}

bool Grid::checkPoint(double x, double y) {
  int i = (int)(x - xmin) / sizex;
  if ((i < 0) || (i >= cellsx)) {
    return false;
  }

  int j = (int)(y - ymin) / sizey;
  if ((j < 0) || (j >= cellsy)) {
    return false;
  }

  int col = cells[i][j]->color;

  if (col == CWHITE) {
    return false;
  }
  if (col == CBLACK) {
    return true;
  }

  int crossings = 0;
  if (col != CSINGULAR) {
    // Iterate edges of cell
    for (PolyEdge* e : cells[i][j]->edges) {
      if (RayLineIntersects(getEdgeToCenter(i, j, x, y), e)) {
        crossings++;
      }
    }
  }
  else {
    // TODO: find a genious way to select the closes not SINGULAR cell
    int i2 = i;
    int sign = 1;

    while (cells[i2][j]->color == CSINGULAR) {
      if (i2 > cellsx) {
        // when end of row is reached start reverse direction
        i2 = i;
        sign = -1;
      }
      if (i2 == 0) {
        throw std::exception("No non-singular cells found in row, adjust code!");
      }
      i2 = i + sign;
    }
    // Iterate edges of cell between first and last
    for (int ii = 0; ii < sign*(i2 - i); i + sign) {
      for (PolyEdge* e : cells[ii][j]->edges) {
        if (RayLineIntersects(getEdgeToCenter(i2, j, x, y), e)) {
          crossings++;
        }
      }
    }
  }
  // equal crossings gives same color for cell
  if (crossings % 2 == 0) {
    if (cells[i][j]->color == CBLACK) {
      return false;
    }
    else if (cells[i][j]->color == CWHITE) {
      return true;
    }
  }
  else {
    // inverse color
    if (cells[i][j]->color == CBLACK) {
      return true;
    }
    else if (cells[i][j]->color == CWHITE) {
      return false;
    }
  }
  return false;
}
