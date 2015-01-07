#ifndef SOFTCASCADE
#define SOFTCASCADE

#include <string>
#include <iostream>
#include <vector>
#include "opencv2/highgui/highgui.hpp"
#include "../Adaboost/Adaboost.hpp"
#include "../binaryTree/binarytree.hpp"

using namespace std;
using namespace cv;


struct cascadeParameter
{
	vector<int> filter;					/* de-correlation filter parameters, eg [5 5]  */
	Size modelDs;						/* model height+width without padding (eg [100 41]) */
	Size modelDsPad;					/* model height+width with padding (eg [128 64])*/
	

	int shrink;							/* ----------> should be provided by the chnPyramid */


	/* ---> TODO non maximun suppression parameters ... */

	int stride;							/* [4] spatial stride between detection windows */
	double cascThr;						/* [-1] constant cascade threshold (affects speed/accuracy)*/
	double cascCal;						/* [.005] cascade calibration (affects speed/accuracy) */
	vector<int> nWeaks;					/* [128] vector defining number weak clfs per stagemodel eg[64 128 256 1024]*/
	int pBoost_nweaks;					/* parameters for boosting */
	tree_para pBoost_pTree;				/* parameters for boosting */
	string infos;						/* other informations~~ */
	int nPos;							/* [-1 -> inf] max number of pos windows to sample */						
	int nNeg;							/* [5000] max number of neg windows to sample*/
	int nPerNeg;						/* [25]  max number of neg windows to sample per image*/
	int nAccNeg;						/* [10000] max number of neg windows to accumulate*/

	/* ---> TODO jitter parameters .... */
	cascadeParameter()
	{
		modelDs    = Size(41, 100);
		modelDsPad = Size(64, 128);
		stride     = 4;
		cascThr	   = -1;
		cascCal    = 0.005;
		nWeaks.push_back( 128);
		pBoost_nweaks = 128;
		pBoost_pTree = tree_para();
		infos = "no infos";
		nPos = -1;
		nNeg = 5000;
		nPerNeg = 25;
		nAccNeg = 10000;

		shrink = 4;
	}
};


class softcascade
{
	public:

		/* 
		 * ===  FUNCTION  ======================================================================
		 *         Name:  Load
		 *  Description:  load models from path
		 * =====================================================================================
		 */
		bool Load( string path_to_model );		/* in : path of the model, shoule be a xml file saved by opencv FileStorage */

		/* 
		 * ===  FUNCTION  ======================================================================
		 *         Name:  Apply
		 *  Description:  predict the result giving data
		 *           in:  input_data, column vectors, same type as training data
		 *          out:  detect result
		 * =====================================================================================
		 */
		bool Apply( const Mat &input_data,		    /*  in: featuredim x number_of_samples */
				    vector<Rect> &results ) const;	/* out: detect results on image */


		/* 
		 * ===  FUNCTION  ======================================================================
		 *         Name:  Predict
		 *  Description:  test a single sample
		 *			 in:  data
		 *			out:  predicted score ( hs ) 
		 *			      !!! this function shoule not be used in sliding window search, since the
		 *			      memory is not continuous for each window Rect
		 * =====================================================================================
		 */
		template < typename T> bool Predict(  T *data, double &score) const
		{
			if(!checkModel())
				return false;
			double h = 0;
			for( int c=0;c<m_number_of_trees;c++)
			{
				int position   = 0;
				const int *t_child   = m_child.ptr<int>(c);
				const int *t_fids    = m_fids.ptr<int>(c);
				const double *t_thrs = m_thrs.ptr<double>(c);
				const double *t_hs   = m_hs.ptr<double>(c);
				while( t_child[position] )  /*  iterate the tree */
				{
					position = (( data[t_fids[position]] < t_thrs[position]) ? t_child[position]: t_child[position] + 1);
				}
				h += t_hs[position];
			}
			score = h;
		}
			
		/* 
		 * ===  FUNCTION  ======================================================================
		 *         Name:  Save
		 *  Description:  save model
		 * =====================================================================================
		 */
		bool Save( string path_to_model );		/*  in: where to save the model, models is saved by opencv FileStorage */

		
		/* 
		 * ===  FUNCTION  ======================================================================
		 *         Name:  Combine
		 *  Description:  combine the adaboost class to a long cascade
		 *				  adaboost  is trained as the stage, each has different number of trees, eg
		 *				  [ad1 ad2 ad3 ad4 ]  -> [32 64 256 1024], now make a long cascade contains 1376 trees
		 * =====================================================================================
		 */
		bool Combine( vector<Adaboost> &ads );


		/* 
		 * ===  FUNCTION  ======================================================================
		 *         Name:  checkModel
		 *  Description:  check if the model is loaded right
		 * =====================================================================================
		 */
		bool checkModel() const;

		private:
			Mat m_fids;							/* nxK 32S feature index for each node , n -> number of trees, K -> number of nodes*/
			Mat m_thrs;							/* nxK 64F thresholds for each node */
			Mat m_child;						/* nxK 32S child index for each node */
			Mat m_hs;							/* nxK 64F log ratio (.5*log(p/(1-p)) at each node  */
			Mat m_weights;						/* nxK 64F total sample weight at each node */
			Mat m_depth;						/* nxK 32S depth of node*/
			Mat m_nodes;						/* nx1 32S number of nodes of each tree */
			bool m_debug;						/* wanna output? */
			cascadeParameter m_opts;            /* detectot options  */
			int m_number_of_trees;				/* . */

};
#endif