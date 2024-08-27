#include <gtest/gtest.h>
#include <iostream>

// ASSERT_TRUE(参数)，期待结果是 true
// ASSERT_FALSE(参数)，期待结果是 false
// ASSERT_EQ(参数 1，参数 2)，传入的是需要比较的两个数 equal
// ASSERT_NE(参数 1，参数 2)，not equal，不等于才返回 true
// ASSERT_LT(参数 1，参数 2)，less than，小于才返回 true
// ASSERT_GT(参数 1，参数 2)，greater than，大于才返回 true
// ASSERT_LE(参数 1，参数 2)，less equal，小于等于才返回 true
// ASSERT_GE(参数 1，参数 2)，greater equal，大于等于才返回 true

int abs(int x)
{ 
 return x > 0 ? x : -x;
}

// 编写单元测试用例
TEST(test1, insert)
{
    ASSERT_TRUE(abs(1) == 1) << "abs(1)=1";
    ASSERT_TRUE(abs(-1) == 1);
    ASSERT_FALSE(abs(-2) == -2);
    ASSERT_EQ(abs(1), abs(-1));
    ASSERT_NE(abs(-1), 0);
    ASSERT_LT(abs(-1), 2);
    ASSERT_GT(abs(-1), 0);
    ASSERT_LE(abs(-1), 2);
    ASSERT_GE(abs(-1), 0);
}

int main(int argc, char *argv[])
{
    // 框架初始化
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}