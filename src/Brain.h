#ifndef BRAIN_H
#define BRAIN_H

#include <mlpack/core.hpp>
#include <mlpack/methods/neighbor_search.hpp>
#include <QString>

class Brain {
public:
    Brain();
    
    // 1-Свежее, 0-Гнилое
    int classify(const arma::rowvec& features);
    void learn(const arma::rowvec& features, int label);

private:
    void load();
    void save();

    arma::mat m_dataset;        
    arma::Row<size_t> m_labels; 
    QString m_path;
};
#endif
