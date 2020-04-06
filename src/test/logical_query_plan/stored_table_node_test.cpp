#include <memory>
#include <string>

#include "base_test.hpp"

#include "expression/expression_functional.hpp"
#include "hyrise.hpp"
#include "logical_query_plan/lqp_utils.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "statistics/table_statistics.hpp"
#include "storage/chunk_encoder.hpp"
#include "storage/constraints/table_constraint_definition.hpp"
#include "storage/index/group_key/composite_group_key_index.hpp"
#include "storage/index/group_key/group_key_index.hpp"
#include "utils/constraint_test_utils.hpp"

using namespace opossum::expression_functional;  // NOLINT

namespace opossum {

class StoredTableNodeTest : public BaseTest {
 protected:
  void SetUp() override {
    Hyrise::get().storage_manager.add_table("t_a", load_table("resources/test_data/tbl/int_int_float.tbl", 1));
    Hyrise::get().storage_manager.add_table("t_b", load_table("resources/test_data/tbl/int_int_float.tbl", 1));

    const auto& table_t_a = Hyrise::get().storage_manager.get_table("t_a");
    ChunkEncoder::encode_all_chunks(table_t_a);
    table_t_a->create_index<GroupKeyIndex>({ColumnID{0}}, "i_a1");
    table_t_a->create_index<GroupKeyIndex>({ColumnID{1}}, "i_b");
    table_t_a->create_index<CompositeGroupKeyIndex>({ColumnID{0}, ColumnID{1}}, "i_a2");
    table_t_a->create_index<CompositeGroupKeyIndex>({ColumnID{1}, ColumnID{0}}, "i_a3");

    _stored_table_node = StoredTableNode::make("t_a");
    _a = LQPColumnReference(_stored_table_node, ColumnID{0});
    _b = LQPColumnReference(_stored_table_node, ColumnID{1});
    _c = LQPColumnReference(_stored_table_node, ColumnID{2});

    _stored_table_node->set_pruned_chunk_ids({ChunkID{2}});
  }

  std::shared_ptr<StoredTableNode> _stored_table_node;
  LQPColumnReference _a, _b, _c;
};

TEST_F(StoredTableNodeTest, Description) {
  const auto stored_table_node_a = StoredTableNode::make("t_a");
  EXPECT_EQ(stored_table_node_a->description(), "[StoredTable] Name: 't_a' pruned: 0/4 chunk(s), 0/3 column(s)");

  const auto stored_table_node_b = StoredTableNode::make("t_a");
  stored_table_node_b->set_pruned_chunk_ids({ChunkID{2}});
  stored_table_node_b->set_pruned_column_ids({ColumnID{1}});
  EXPECT_EQ(stored_table_node_b->description(), "[StoredTable] Name: 't_a' pruned: 1/4 chunk(s), 1/3 column(s)");
}

TEST_F(StoredTableNodeTest, GetColumn) {
  EXPECT_EQ(_stored_table_node->get_column("a"), _a);
  EXPECT_EQ(_stored_table_node->get_column("b"), _b);

  // Column pruning does not interfere with get_column()
  _stored_table_node->set_pruned_column_ids({ColumnID{0}});
  EXPECT_EQ(_stored_table_node->get_column("a"), _a);
  EXPECT_EQ(_stored_table_node->get_column("b"), _b);
}

TEST_F(StoredTableNodeTest, ColumnExpressions) {
  EXPECT_EQ(_stored_table_node->column_expressions().size(), 3u);
  EXPECT_EQ(*_stored_table_node->column_expressions().at(0u), *lqp_column_(_a));
  EXPECT_EQ(*_stored_table_node->column_expressions().at(1u), *lqp_column_(_b));
  EXPECT_EQ(*_stored_table_node->column_expressions().at(2u), *lqp_column_(_c));

  // Column pruning does not interfere with get_column()
  _stored_table_node->set_pruned_column_ids({ColumnID{0}});
  EXPECT_EQ(_stored_table_node->column_expressions().size(), 2u);
  EXPECT_EQ(*_stored_table_node->column_expressions().at(0u), *lqp_column_(_b));
  EXPECT_EQ(*_stored_table_node->column_expressions().at(1u), *lqp_column_(_c));
}

TEST_F(StoredTableNodeTest, HashingAndEqualityCheck) {
  EXPECT_EQ(*_stored_table_node, *_stored_table_node);

  const auto different_node_a = StoredTableNode::make("t_b");
  different_node_a->set_pruned_chunk_ids({ChunkID{2}});

  const auto different_node_b = StoredTableNode::make("t_a");

  const auto different_node_c = StoredTableNode::make("t_b");
  different_node_c->set_pruned_column_ids({ColumnID{1}});
  const auto different_node_c2 = StoredTableNode::make("t_b");
  different_node_c2->set_pruned_column_ids({ColumnID{1}});

  EXPECT_NE(*_stored_table_node, *different_node_a);
  EXPECT_NE(*_stored_table_node, *different_node_b);
  EXPECT_NE(*_stored_table_node, *different_node_c);
  EXPECT_EQ(*different_node_c, *different_node_c2);

  EXPECT_NE(_stored_table_node->hash(), different_node_a->hash());
  EXPECT_NE(_stored_table_node->hash(), different_node_b->hash());
  EXPECT_NE(_stored_table_node->hash(), different_node_c->hash());
  EXPECT_EQ(different_node_c->hash(), different_node_c2->hash());
}

TEST_F(StoredTableNodeTest, Copy) {
  EXPECT_EQ(*_stored_table_node->deep_copy(), *_stored_table_node);

  _stored_table_node->set_pruned_chunk_ids({ChunkID{2}});
  _stored_table_node->set_pruned_column_ids({ColumnID{1}});
  EXPECT_EQ(*_stored_table_node->deep_copy(), *_stored_table_node);
}

TEST_F(StoredTableNodeTest, NodeExpressions) { ASSERT_EQ(_stored_table_node->node_expressions.size(), 0u); }

TEST_F(StoredTableNodeTest, GetStatisticsPruneFirstColumn) {
  EXPECT_EQ(_stored_table_node->indexes_statistics().size(), 4u);

  auto expected_statistics = _stored_table_node->indexes_statistics().at(1u);

  _stored_table_node->set_pruned_column_ids({ColumnID{0}});

  // column with ColumnID{0} was pruned, therefore the column has to be left shifted
  expected_statistics.column_ids[0] -= 1;

  EXPECT_EQ(_stored_table_node->indexes_statistics().size(), 1u);
  EXPECT_EQ(_stored_table_node->indexes_statistics().at(0u), expected_statistics);
}

TEST_F(StoredTableNodeTest, GetStatisticsPruneSecondColumn) {
  EXPECT_EQ(_stored_table_node->indexes_statistics().size(), 4u);

  auto expected_statistics = _stored_table_node->indexes_statistics().at(0u);

  _stored_table_node->set_pruned_column_ids({ColumnID{1}});

  // column with ColumnID{1} was pruned, so ColumnID{0} should be untouched

  EXPECT_EQ(_stored_table_node->indexes_statistics().size(), 1u);
  EXPECT_EQ(_stored_table_node->indexes_statistics().at(0u), expected_statistics);
}

TEST_F(StoredTableNodeTest, GetStatisticsPruneBothColumns) {
  EXPECT_EQ(_stored_table_node->indexes_statistics().size(), 4u);

  _stored_table_node->set_pruned_column_ids({ColumnID{0}, ColumnID{1}});

  // All indexed columns were pruned, therefore the index statistics should be empty
  EXPECT_EQ(_stored_table_node->indexes_statistics().size(), 0u);
}

TEST_F(StoredTableNodeTest, Constraints) {
  const auto table = Hyrise::get().storage_manager.get_table("t_a");

  table->add_soft_unique_constraint(TableConstraintDefinition{{ColumnID{0}, ColumnID{1}}, IsPrimaryKey::Yes});
  table->add_soft_unique_constraint(TableConstraintDefinition{{ColumnID{2}}, IsPrimaryKey::No});

  const auto table_constraints = table->get_soft_unique_constraints();
  const auto lqp_constraints = _stored_table_node->constraints();

  // Basic check
  EXPECT_EQ(table_constraints.size(), 2);
  EXPECT_EQ(lqp_constraints->size(), 2);

  // In-depth - check whether all table constraints are represented in StoredTableNode
  check_table_constraint_representation(table_constraints, lqp_constraints);

  // Also check whether StoredTableNode is referenced correctly by column expressions
  for (const auto& lqp_constraint : *lqp_constraints) {
    for (const auto& expr : lqp_constraint.column_expressions) {
      const auto& column_expr = std::dynamic_pointer_cast<LQPColumnExpression>(expr);
      EXPECT_EQ(column_expr->column_reference.original_node(), _stored_table_node);
    }
  }
}

TEST_F(StoredTableNodeTest, ConstraintsPrunedColumns) {
  const auto table = Hyrise::get().storage_manager.get_table("t_a");

  const auto table_constraint_1 = TableConstraintDefinition{{ColumnID{0}}};
  const auto table_constraint_2 = TableConstraintDefinition{{ColumnID{0}, ColumnID{1}}};
  const auto table_constraint_3 = TableConstraintDefinition{{ColumnID{2}}};
  table->add_soft_unique_constraint(table_constraint_1);
  table->add_soft_unique_constraint(table_constraint_2);
  table->add_soft_unique_constraint(table_constraint_3);
  _stored_table_node->set_pruned_column_ids({ColumnID{0}});

  const auto& table_constraints = table->get_soft_unique_constraints();
  EXPECT_EQ(table_constraints.size(), 3);

  // After column pruning, only the third table constraint should remain valid (the one based on ColumnID 2)
  // Basic check
  const auto lqp_constraints = _stored_table_node->constraints();
  EXPECT_EQ(lqp_constraints->size(), 1);

  // In-depth check
  const auto& valid_table_constraint = table_constraint_3;
  check_table_constraint_representation(TableConstraintDefinitions{valid_table_constraint}, lqp_constraints);
}

TEST_F(StoredTableNodeTest, ConstraintsEmpty) {
  EXPECT_TRUE(
      Hyrise::get().storage_manager.get_table(_stored_table_node->table_name)->get_soft_unique_constraints().empty());
  EXPECT_TRUE(_stored_table_node->constraints()->empty());
}

TEST_F(StoredTableNodeTest, FunctionalDependenciesNone) {
  // No constraints => No functional dependencies
  EXPECT_TRUE(_stored_table_node->functional_dependencies().empty());

  // Constraint across all columns => No more columns available to create a functional dependency from
  const auto table = Hyrise::get().storage_manager.get_table("t_a");
  table->add_soft_unique_constraint(TableConstraintDefinition{
      {_a.original_column_id(), _b.original_column_id(), _c.original_column_id()}});

  EXPECT_TRUE(_stored_table_node->functional_dependencies().empty());
}

TEST_F(StoredTableNodeTest, FunctionalDependenciesSingle) {
  const auto table = Hyrise::get().storage_manager.get_table("t_a");
  table->add_soft_unique_constraint(TableConstraintDefinition{{_a.original_column_id()}});

  const auto& fds = _stored_table_node->functional_dependencies();
  EXPECT_TRUE(fds.size() == 1);
  const auto fd = fds.at(0);

  // Check left
  EXPECT_EQ(fd.first.size(), 1);
  EXPECT_TRUE(fd.first.contains(lqp_column_(_a)));

  // Check right
  EXPECT_TRUE(fd.second.size() == 2);
  EXPECT_TRUE(fd.second.contains(lqp_column_(_b)));
  EXPECT_TRUE(fd.second.contains(lqp_column_(_c)));
}

TEST_F(StoredTableNodeTest, FunctionalDependenciesMultiple) {
  const auto table = Hyrise::get().storage_manager.get_table("t_a");
  table->add_soft_unique_constraint(TableConstraintDefinition{{
    _a.original_column_id()}});
  table->add_soft_unique_constraint(TableConstraintDefinition{{
    _a.original_column_id(), _b.original_column_id()}});

  const auto& fds = _stored_table_node->functional_dependencies();
  EXPECT_TRUE(fds.size() == 2);
  const auto fd1 = fds.at(0);
  const auto fd2 = fds.at(1);

  // Check left of fd1
  EXPECT_EQ(fd1.first.size(), 1);
  EXPECT_TRUE(fd1.first.contains(lqp_column_(_a)));

  // Check right of fd1
  EXPECT_EQ(fd1.second.size(), 2);
  EXPECT_TRUE(fd1.second.contains(lqp_column_(_b)));
  EXPECT_TRUE(fd1.second.contains(lqp_column_(_c)));

  // Check left of fd2
  EXPECT_EQ(fd2.first.size(), 2);
  EXPECT_TRUE(fd2.first.contains(lqp_column_(_a)));
  EXPECT_TRUE(fd2.first.contains(lqp_column_(_b)));

  // Check right of fd2
  EXPECT_EQ(fd2.second.size(), 1);
  EXPECT_TRUE(fd2.second.contains(lqp_column_(_c)));
}

TEST_F(StoredTableNodeTest, FunctionalDependenciesExcludeNullableColumns) {
  // Create a tables of 3 columns (a, b, c) where the last column of which is nullable (c)
  TableColumnDefinitions column_definitions{{"a", DataType::Int, false},
      {"b", DataType::Int, false},
      {"c", DataType::Int, true}};
  const auto table_a = std::make_shared<Table>(column_definitions, TableType::Data);
  const auto table_b = std::make_shared<Table>(column_definitions, TableType::Data);
  Hyrise::get().storage_manager.add_table("t_a_nullable", table_a);
  Hyrise::get().storage_manager.add_table("t_b_nullable", table_b);

  // Add unique constraints
  table_a->add_soft_unique_constraint({{ColumnID{0}}});
  table_a->add_soft_unique_constraint({{ColumnID{0}, ColumnID{1}}});
  table_b->add_soft_unique_constraint({{ColumnID{2}}});

  const auto stored_table_node_a = StoredTableNode::make("t_a_nullable");
  const auto stored_table_node_b = StoredTableNode::make("t_b_nullable");
  const auto& fds_a = stored_table_node_a->functional_dependencies();
  const auto& fds_b = stored_table_node_b->functional_dependencies();
  EXPECT_EQ(fds_a.size(), 0); // without nullability we should get 2 FDs (a => b, c), (a,b => c)
  EXPECT_EQ(fds_b.size(), 0); // without nullability we should get 1 FDs (c => a, b)
}

}  // namespace opossum
