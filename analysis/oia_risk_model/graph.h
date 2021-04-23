#ifndef GRAPH_H
#define GRAPH_H

#include <vector>

namespace oia_risk_model{
  namespace fragility{

    // Structure to hold the RP graph, and do integration under the curve by parts...
    struct Graph{
      std::vector<double> X;  // x-values...
      std::vector<double> Y;  // y-values...
      Graph(std::vector<int> rp, std::vector<double> pFail){
        // We need to add an "anchor point" to the start of the graph, which is taken to be 0...
        X.push_back(0); Y.push_back(0);

        // ...followed by all the points that have been specified...
        for(int i=rp.size()-1; i>=0; i--){
          X.push_back(1.0/double(rp.at(i)));
          Y.push_back(pFail.at(i));
        }

        // ...and finally, we need to add a second "anchor point" on the end of the graph so our integral is closed.
        if(X.at(X.size()-1) < 1){
          X.push_back(1);
          if(Y.at(Y.size()-1) >= 1)
            Y.push_back(1);
          else
            Y.push_back(0);
        }
      }
      // Helper function to perform trapezoidal integration to calculate the area under the graph...
      double area(){
        double a = 0;
        // Simple integration by parts...
        for(int i=0; i<X.size()-1; i++){
          // Calculate the base-length between adjacent values...
          double base = X.at(i+1) - X.at(i);
          // Calculate the height of the segment...
          double h1   = Y.at(i);
          double h2   = Y.at(i+1);
          // Accumulate the area...
          a += (base * (h1 + h2) / 2.0);
        }

        return a;
      }
    };
  } // fragility
} // oia_risk_model

#endif // GRAPH_H