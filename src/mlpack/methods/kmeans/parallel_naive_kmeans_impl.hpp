/**
 * @file parallel_naive_kmeans_impl.hpp
 * @author Ryan Curtin
 * @author Shikhar Bhardwaj
 *
 * An implementation of a naively-implemented step of the Lloyd algorithm for
 * k-means clustering, using OpenMP for parallelization over multiple threads.
 * This may still be the best choice for small datasets or datasets with very
 * high dimensionality.
 *
 * mlpack is free software; you may redistribute it and/or modify it under the
 * terms of the 3-clause BSD license.  You should have received a copy of the
 * 3-clause BSD license along with mlpack.  If not, see
 * http://www.opensource.org/licenses/BSD-3-Clause for more information.
 */
#ifndef MLPACK_METHODS_KMEANS_PARALLEL_NAIVE_KMEANS_IMPL_HPP
#define MLPACK_METHODS_KMEANS_PARALLEL_NAIVE_KMEANS_IMPL_HPP

// In case it hasn't been included yet.
#include "parallel_naive_kmeans.hpp"

namespace mlpack {
namespace kmeans {

template<typename MetricType, typename MatType>
ParallelNaiveKMeans<MetricType, MatType>::ParallelNaiveKMeans(
    const MatType& dataset, MetricType& metric) :
    dataset(dataset),
    metric(metric),
    distanceCalculations(0)
{ /* Nothing to do. */ }

// Run a single iteration.
template<typename MetricType, typename MatType>
double ParallelNaiveKMeans<MetricType, MatType>::Iterate(
                                                  const arma::mat& centroids,
                                                  arma::mat& newCentroids,
                                                  arma::Col<size_t>& counts)
{
  newCentroids.zeros(centroids.n_rows, centroids.n_cols);
  counts.zeros(centroids.n_cols);

  // Find the closest centroid to each point and update the new centroids.
  // Computed in parallel over the complete dataset
  #pragma omp parallel
  {
    // The current state of the K-means is private for each thread
    arma::mat localCentroids(centroids.n_rows, centroids.n_cols,
                             arma::fill::zeros);
    arma::Col<size_t> localCounts(centroids.n_cols, arma::fill::zeros);

#ifdef _WIN32
  // Tiny workaround: Visual Studio only implements OpenMP 2.0, which doesn't
  // support unsigned loop variables. If we're building for Visual Studio, use
  // the intmax_t type instead.
  #pragma omp for
    for (intmax_t i = 0; i < (intmax_t) dataset.n_cols; ++i)
#else
  #pragma omp for
    for (size_t i = 0; i < dataset.n_cols; ++i)
#endif
    {
      // Find the closest centroid to this point.
      double minDistance = std::numeric_limits<double>::infinity();
      size_t closestCluster = centroids.n_cols; // Invalid value.

      for (size_t j = 0; j < centroids.n_cols; j++)
      {
        const double distance = metric.Evaluate(dataset.col(i),
                                                centroids.col(j));
        if (distance < minDistance)
        {
          minDistance = distance;
          closestCluster = j;
        }
      }

      Log::Assert(closestCluster != centroids.n_cols);

      // We now have the minimum distance centroid index.  Update that centroid.
      localCentroids.col(closestCluster) += arma::vec(dataset.col(i));
      localCounts(closestCluster)++;
    }
    // Combine calculated state from each thread
    #pragma omp critical
    {
      newCentroids += localCentroids;
      counts += localCounts;
    }
  }
  // Now normalize the centroid.
  for (size_t i = 0; i < centroids.n_cols; ++i)
    if (counts(i) != 0)
      newCentroids.col(i) /= counts(i);

  distanceCalculations += centroids.n_cols * dataset.n_cols;

  // Calculate cluster distortion for this iteration.
  double cNorm = 0.0;
  for (size_t i = 0; i < centroids.n_cols; ++i)
  {
    cNorm += std::pow(metric.Evaluate(centroids.col(i), newCentroids.col(i)),
        2.0);
  }
  distanceCalculations += centroids.n_cols;

  return std::sqrt(cNorm);
}

} // namespace kmeans
} // namespace mlpack

#endif
