#pragma once
#include <iostream>
using namespace std;
#include "preconditioner.h"
#include "maxheap.h"
#include "graph.h"


#include "CoarseSolver.h"

#define AMG_MAX_LEVELS 32


template<typename mat_type>
class amg : public preconditioner<mat_type>
{
public:
	typedef matrix<mat_type> matrix_type;
	typedef typename matrix_type::vec_type vec_type;
	typedef Vector< typename matrix_type::vec_type> Vector_type;
	
//  functions
	void writeMatrices(const char *pathAndName);
	amg() 
	{
		used_levels = 0;
		max_levels = 10;
		aggressiveCoarsening = 0;
		aggressiveCoarseningNrOfPaths = 2; // A2
	}
	~amg();
	virtual bool init(const matrix_type& A);

	virtual void precond2(Vector_type *px, const Vector_type &b)
	{
		*px = 0.0;
		MGCycle(px, b, 0);
	}
	virtual double iterate(Vector_type *px, const Vector_type &b)
	{
		return MGCycle(px, b, 0);		
	}
	
	double MGCycle(Vector_type *x, const Vector_type &b, int i=0);	
	void printCoarsening(int level, int n);
	
	void interpolate(Vector_type *pto, const Vector_type &from, int tolevel); // 2h -> h prolongate
	void restriction(Vector_type *pto, const Vector_type &from, int tolevel); // h -> 2h
	
	
	bool onlyOneLevel(const matrix_type& A_);
	
	//void restrictioninjection(Vector_type &to, const Vector_type &from, int tolevel);
	
	int getNrOfCoarse(int level)
	{
		ASSERT(level+1 < used_levels);
		return A[level+1]->length;
	}
	
	int getNrOfUsedLevels() { return used_levels; }
//  data
	int max_levels;
	int aggressiveCoarsening;
	int aggressiveCoarseningNrOfPaths;
	
	void setAggressiveCoarsening_A_2() { aggressiveCoarsening = true; aggressiveCoarseningNrOfPaths = 2;}
	void setAggressiveCoarsening_A_1() { aggressiveCoarsening = true; aggressiveCoarseningNrOfPaths = 1;}
	
private:
	//  structs
#define ASSIGNED_RATING			(-2000000000)
#define COARSE_RATING			(-2000000001)
#define FINE_RATING_INDIRECT	(-2000000002)
#define FINE_RATING				(-2000000003)
	struct nodeinfo
	{
		int rating;
		//int newIndex;		
		inline void setAssigned(){rating = ASSIGNED_RATING;}		
		inline void setCoarse()	{rating = COARSE_RATING;}
		inline void setFine(){rating = FINE_RATING;	}
		inline void setFineIndirect(){rating = FINE_RATING_INDIRECT;}
		inline bool isCoarse(){	return rating == COARSE_RATING;}
		inline bool isFine(){return (rating == FINE_RATING) || (rating == FINE_RATING_INDIRECT);}
		inline bool isFineNotIndirect()	{return (rating == FINE_RATING);}
		inline bool isFineIndirect(){ return rating == FINE_RATING_INDIRECT;}		
		inline bool isAssigned(){return (rating <= ASSIGNED_RATING);}
	
		friend ostream &operator << (ostream &out, nodeinfo &n)
		{
			out << "Rating: " << n.rating;
			if(n.isFineNotIndirect()) out << " (fine not ind)";
			else if(n.isFineIndirect()) out << " (fine ind)";
			else if(n.isCoarse()) out << " (coarse)";
			out << " ";
			return out;
		}
		void print()
		{
			cout << *this << endl;
		} // << " newindex: " << newIndex << endl;
		
		inline bool operator > (const nodeinfo &other)
		{
			if(rating == other.rating)
				return this < &other;
			else
				return rating > other.rating;
		}
	};
	
private:
//  functions
	int getNodeWithBestRating(int n);
		
	void createAMGLevel(matrix_type &AH, matrix<double> &R, const matrix_type &A, matrix<double> &P, int level);
	void createGalerkinMatrix(matrix_type &AH, const matrix<double> &R, const matrix_type &A, const matrix<double> &P, int *posInConnections);
	
	void CreateProlongation(matrix<double> &P, const matrix_type &A, int *newIndex);
	void CreateIndirectProlongation(matrix<double> &P, const matrix_type &A, int *newIndex, int *posInConnections);
	void CreateGraph(const matrix_type &A, cgraph &graph, maxheap<nodeinfo> &PQ, int &unassigned);
	void CreateGraph2(cgraph &graph, cgraph &graph2, maxheap<nodeinfo> &PQ, int &unassigned, int *posInConnections);
	int Coarsen(cgraph &graph, maxheap<nodeinfo> &PQ, int *newIndex, int unassigned, bool bIndirect);
	

//	data
	CoarseSolver coarseSolver;
	nodeinfo *grid;
	int used_levels;
	Vector_type *vec1[AMG_MAX_LEVELS];
	Vector_type *vec2[AMG_MAX_LEVELS];
	Vector_type *vec3[AMG_MAX_LEVELS];
	
	matrix<double> R[AMG_MAX_LEVELS];
	matrix<double> P[AMG_MAX_LEVELS];
	matrix_type *A[AMG_MAX_LEVELS+1];
	
	sgs<mat_type> smoother[AMG_MAX_LEVELS];

};


#include "amg.hpp"