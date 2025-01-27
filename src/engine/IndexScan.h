// Copyright 2015, University of Freiburg,
// Chair of Algorithms and Data Structures.
// Author: Björn Buchhold (buchhold@informatik.uni-freiburg.de)
#pragma once

#include <string>

#include "./Operation.h"

using std::string;

class SparqlTriple;

class IndexScan : public Operation {
 private:
  Permutation::Enum permutation_;
  TripleComponent subject_;
  TripleComponent predicate_;
  TripleComponent object_;
  size_t numVariables_;
  size_t sizeEstimate_;
  vector<float> multiplicity_;

 public:
  string getDescriptor() const override;

  IndexScan(QueryExecutionContext* qec, Permutation::Enum permutation,
            const SparqlTriple& triple);

  virtual ~IndexScan() = default;

  const TripleComponent& getPredicate() const { return predicate_; }
  const TripleComponent& getSubject() const { return subject_; }
  const TripleComponent& getObject() const { return object_; }

  size_t getResultWidth() const override;

  vector<ColumnIndex> resultSortedOn() const override;

  void setTextLimit(size_t) override {
    // Do nothing.
  }

  // Return the exact result size of the index scan. This is always known as it
  // can be read from the Metadata.
  size_t getExactSize() const { return sizeEstimate_; }

  // Return two generators that lazily yield the results of `s1` and `s2` in
  // blocks, but only the blocks that can theoretically contain matching rows
  // when performing a join on the first column of the result of `s1` with the
  // first column of the result of `s2`.
  static std::array<Permutation::IdTableGenerator, 2> lazyScanForJoinOfTwoScans(
      const IndexScan& s1, const IndexScan& s2);

  // Return a generator that lazily yields the result of `s` in blocks, but only
  // the blocks that can theoretically contain matching rows when performing a
  // join between the first column of the result of `s`  with the `joinColumn`.
  // Requires that the `joinColumn` is sorted, else the behavior is undefined.
  static Permutation::IdTableGenerator lazyScanForJoinOfColumnWithScan(
      std::span<const Id> joinColumn, const IndexScan& s);

 private:
  // TODO<joka921> Make the `getSizeEstimateBeforeLimit()` function `const` for
  // ALL the `Operations`.
  uint64_t getSizeEstimateBeforeLimit() override { return getExactSize(); }

 public:
  size_t getCostEstimate() override;

  void determineMultiplicities();

  float getMultiplicity(size_t col) override {
    if (multiplicity_.empty()) {
      determineMultiplicities();
    }
    assert(col < multiplicity_.size());
    return multiplicity_[col];
  }

  bool knownEmptyResult() override { return getExactSize() == 0; }

  // Currently only the full scans support a limit clause.
  [[nodiscard]] bool supportsLimit() const override {
    return getResultWidth() == 3;
  }

  Permutation::Enum permutation() const { return permutation_; }

 private:
  ResultTable computeResult() override;

  vector<QueryExecutionTree*> getChildren() override { return {}; }

  void computeFullScan(IdTable* result, Permutation::Enum permutation) const;

  size_t computeSizeEstimate();

  string asStringImpl(size_t indent) const override;

  VariableToColumnMap computeVariableToColumnMap() const override;

  // Return the stored triple in the order that corresponds to the
  // `permutation_`. For example if `permutation_ == PSO` then the result is
  // {&predicate_, &subject_, &object_}
  std::array<const TripleComponent* const, 3> getPermutedTriple() const;

  //  Helper functions for the public `getLazyScanFor...` functions (see above).
  static Permutation::IdTableGenerator getLazyScan(
      const IndexScan& s, std::vector<CompressedBlockMetadata> blocks);
  static std::optional<Permutation::MetadataAndBlocks> getMetadataForScan(
      const IndexScan& s);
};
