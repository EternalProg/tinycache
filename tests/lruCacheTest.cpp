#include <gtest/gtest.h>
#include <chrono>
#include <lruCache.hpp>
#include <thread>

using tinycache::LruCache;

class LruCacheTest : public ::testing::Test {
 protected:
  LruCache cache_{100};  // Small capacity for testing eviction
};

// Basic Get/Set tests
TEST_F(LruCacheTest, SetAndGetReturnsValue) {
  cache_.set("key1", "value1");
  auto result = cache_.get("key1");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "value1");
}

TEST_F(LruCacheTest, GetNonExistentKeyReturnsNullopt) {
  auto result = cache_.get("nonexistent");
  EXPECT_FALSE(result.has_value());
}

TEST_F(LruCacheTest, SetMultipleKeysAndGet) {
  cache_.set("key1", "value1");
  cache_.set("key2", "value2");
  cache_.set("key3", "value3");

  EXPECT_EQ(cache_.get("key1").value(), "value1");
  EXPECT_EQ(cache_.get("key2").value(), "value2");
  EXPECT_EQ(cache_.get("key3").value(), "value3");
}

TEST_F(LruCacheTest, OverwriteExistingKey) {
  cache_.set("key1", "value1");
  cache_.set("key1", "updated_value");

  auto result = cache_.get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "updated_value");
}

// Deletion tests
TEST_F(LruCacheTest, DeleteExistingKey) {
  cache_.set("key1", "value1");
  bool deleted = cache_.del("key1");

  EXPECT_TRUE(deleted);
  EXPECT_FALSE(cache_.get("key1").has_value());
}

TEST_F(LruCacheTest, DeleteNonExistentKeyReturnsFalse) {
  bool deleted = cache_.del("nonexistent");
  EXPECT_FALSE(deleted);
}

TEST_F(LruCacheTest, DeleteOneKeyDoesNotAffectOthers) {
  cache_.set("key1", "value1");
  cache_.set("key2", "value2");
  cache_.set("key3", "value3");

  bool deleted = cache_.del("key2");
  EXPECT_TRUE(deleted);

  EXPECT_TRUE(cache_.get("key1").has_value());
  EXPECT_FALSE(cache_.get("key2").has_value());
  EXPECT_TRUE(cache_.get("key3").has_value());
}

// LRU Eviction tests
TEST_F(LruCacheTest, EvictsLeastRecentlyUsedWhenFull) {
  LruCache small_cache{3};  // Capacity of 3

  small_cache.set("key1", "value1");
  small_cache.set("key2", "value2");
  small_cache.set("key3", "value3");

  // Cache is now full, adding key4 should evict key1 (least recently used)
  small_cache.set("key4", "value4");

  EXPECT_FALSE(small_cache.get("key1").has_value());  // key1 should be evicted
  EXPECT_TRUE(small_cache.get("key2").has_value());
  EXPECT_TRUE(small_cache.get("key3").has_value());
  EXPECT_TRUE(small_cache.get("key4").has_value());
}

TEST_F(LruCacheTest, AccessingKeyMovesItToFront) {
  LruCache small_cache{3};

  small_cache.set("key1", "value1");
  small_cache.set("key2", "value2");
  small_cache.set("key3", "value3");

  // Access key1 to make it recently used
  small_cache.get("key1");

  // Add key4, should evict key2 (now the least recently used)
  small_cache.set("key4", "value4");

  EXPECT_TRUE(
      small_cache.get("key1").has_value());  // key1 was accessed, so it stays
  EXPECT_FALSE(small_cache.get("key2").has_value());  // key2 should be evicted
  EXPECT_TRUE(small_cache.get("key3").has_value());
  EXPECT_TRUE(small_cache.get("key4").has_value());
}

TEST_F(LruCacheTest, UpdatingKeyMovesItToFront) {
  LruCache small_cache{3};

  small_cache.set("key1", "value1");
  small_cache.set("key2", "value2");
  small_cache.set("key3", "value3");

  // Update key1 to make it recently used
  small_cache.set("key1", "updated_value1");

  // Add key4, should evict key2 (now the least recently used)
  small_cache.set("key4", "value4");

  EXPECT_TRUE(
      small_cache.get("key1").has_value());  // key1 was updated, so it stays
  EXPECT_FALSE(small_cache.get("key2").has_value());  // key2 should be evicted
  EXPECT_TRUE(small_cache.get("key3").has_value());
  EXPECT_TRUE(small_cache.get("key4").has_value());
}

TEST_F(LruCacheTest, MultipleEvictionsWithAccess) {
  LruCache small_cache{3};

  small_cache.set("key1", "value1");
  small_cache.set("key2", "value2");
  small_cache.set("key3", "value3");
  // Current state: [key3, key2, key1] (front to back)

  // Access key1 to make it most recently used
  (void)small_cache.get("key1");
  // Current state: [key1, key3, key2]

  // Add key4, should evict key2 (least recently used)
  small_cache.set("key4", "value4");
  // Current state: [key4, key1, key3]
  EXPECT_FALSE(small_cache.get("key2").has_value());  // key2 was evicted
  EXPECT_TRUE(small_cache.get("key1").has_value());
  EXPECT_TRUE(small_cache.get("key3").has_value());
  EXPECT_TRUE(small_cache.get("key4").has_value());
}

// TTL/Expiration tests
TEST_F(LruCacheTest, ExpireAndTtlReturnNoExpirationInitially) {
  cache_.set("key1", "value1");
  std::int64_t ttl = cache_.ttl("key1");

  EXPECT_EQ(ttl, -1);  // -1 means no expiration
}

TEST_F(LruCacheTest, TtlReturnsNegativeTwoForNonExistentKey) {
  std::int64_t ttl = cache_.ttl("nonexistent");
  EXPECT_EQ(ttl, -2);  // -2 means key doesn't exist
}

TEST_F(LruCacheTest, ExpireSetsTtl) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 10);

  std::int64_t ttl = cache_.ttl("key1");
  EXPECT_GE(ttl, 9);  // Should be around 10, accounting for execution time
  EXPECT_LE(ttl, 10);
}

TEST_F(LruCacheTest, ExpireOnNonExistentKeyDoesNothing) {
  cache_.expire("nonexistent", 10);
  std::int64_t ttl = cache_.ttl("nonexistent");
  EXPECT_EQ(ttl, -2);  // Still non-existent
}

TEST_F(LruCacheTest, ExpiredKeyReturnsNullopt) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 1);  // Expire in 1 second

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  auto result = cache_.get("key1");
  EXPECT_FALSE(result.has_value());
}

TEST_F(LruCacheTest, ExpiredKeyTtlReturnsNegativeTwo) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 1);  // Expire in 1 second

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  std::int64_t ttl = cache_.ttl("key1");
  EXPECT_EQ(ttl, -2);
}

TEST_F(LruCacheTest, SettingValueClearsExpiration) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 1);

  // Update the value
  cache_.set("key1", "updated_value");

  // Sleep and check that key still exists (expiration was cleared)
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  auto result = cache_.get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "updated_value");
}

TEST_F(LruCacheTest, TtlDecreasesOverTime) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 10);

  std::int64_t ttl1 = cache_.ttl("key1");
  std::this_thread::sleep_for(
      std::chrono::milliseconds(1500));  // Sleep 1.5 seconds
  std::int64_t ttl2 = cache_.ttl("key1");

  EXPECT_GT(ttl1, ttl2);  // ttl should decrease by at least 1 second
}

// Empty string and edge case tests
TEST_F(LruCacheTest, StoresEmptyString) {
  cache_.set("key1", "");
  auto result = cache_.get("key1");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "");
}

TEST_F(LruCacheTest, StoresEmptyKey) {
  cache_.set("", "value1");
  auto result = cache_.get("");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "value1");
}

TEST_F(LruCacheTest, StoresLargeValues) {
  std::string large_value(10000, 'x');
  cache_.set("key1", large_value);

  auto result = cache_.get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), large_value);
  EXPECT_EQ(result.value().size(), 10000);
}

TEST_F(LruCacheTest, StoresSpecialCharacters) {
  std::string special = "!@#$%^&*()_+-=[]{}|;:',.<>?/~`";
  cache_.set("key1", special);

  auto result = cache_.get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), special);
}

// Default capacity tests
TEST_F(LruCacheTest, DefaultCapacityIs1024) {
  LruCache default_cache;
  EXPECT_EQ(default_cache.kDefaultCapacity, 1024);

  // Fill cache beyond reasonable test limits (just verify it doesn't crash)
  for (int i = 0; i < 100; ++i) {
    default_cache.set("key" + std::to_string(i), "value" + std::to_string(i));
  }

  // Should still be able to get recent items
  EXPECT_TRUE(default_cache.get("key99").has_value());
}

// Boundary tests
TEST_F(LruCacheTest, CapacityOfOne) {
  LruCache tiny_cache{1};

  tiny_cache.set("key1", "value1");
  EXPECT_TRUE(tiny_cache.get("key1").has_value());

  tiny_cache.set("key2", "value2");
  EXPECT_FALSE(tiny_cache.get("key1").has_value());
  EXPECT_TRUE(tiny_cache.get("key2").has_value());
}

TEST_F(LruCacheTest, DeleteThenReInsert) {
  cache_.set("key1", "value1");
  bool deleted = cache_.del("key1");
  EXPECT_TRUE(deleted);
  cache_.set("key1", "new_value1");

  auto result = cache_.get("key1");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "new_value1");
}

TEST_F(LruCacheTest, AccessAfterExpiration) {
  cache_.set("key1", "value1");
  cache_.expire("key1", 1);

  std::this_thread::sleep_for(std::chrono::milliseconds(1100));

  // First access should return nullopt and clean up
  auto result = cache_.get("key1");
  EXPECT_FALSE(result.has_value());

  // Second access should also return nullopt
  result = cache_.get("key1");
  EXPECT_FALSE(result.has_value());
}

// Mix of operations
TEST_F(LruCacheTest, ComplexMixOfOperations) {
  LruCache small_cache{4};

  // Set 4 items
  small_cache.set("key1", "value1");
  small_cache.set("key2", "value2");
  small_cache.set("key3", "value3");
  small_cache.set("key4", "value4");
  // Order: [key4, key3, key2, key1]

  // Access key1 to make it most recent
  (void)small_cache.get("key1");
  // Order: [key1, key4, key3, key2]

  // Delete key3
  bool deleted = small_cache.del("key3");
  EXPECT_TRUE(deleted);
  // Order: [key1, key4, key2]

  // Add key5 - cache now has 4 items (key1, key4, key2, key5)
  small_cache.set("key5", "value5");
  // Order: [key5, key1, key4, key2]

  EXPECT_TRUE(small_cache.get("key1").has_value());
  EXPECT_TRUE(small_cache.get("key2").has_value());   // Still there
  EXPECT_FALSE(small_cache.get("key3").has_value());  // Was deleted
  EXPECT_TRUE(small_cache.get("key4").has_value());
  EXPECT_TRUE(small_cache.get("key5").has_value());

  // Expire key4
  small_cache.expire("key4", 1);

  // Verify it expires
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  EXPECT_FALSE(small_cache.get("key4").has_value());

  // key1 and key5 and key2 should still be there
  EXPECT_TRUE(small_cache.get("key1").has_value());
  EXPECT_TRUE(small_cache.get("key2").has_value());
  EXPECT_TRUE(small_cache.get("key5").has_value());
}
