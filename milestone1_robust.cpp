#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <memory>

namespace pip {

constexpr double EPS = 1e-9;

struct Point {
    double x{};
    double y{};
};

struct BoundingBox {
    double minX{std::numeric_limits<double>::infinity()};
    double minY{std::numeric_limits<double>::infinity()};
    double maxX{-std::numeric_limits<double>::infinity()};
    double maxY{-std::numeric_limits<double>::infinity()};

    bool isValid() const {
        return minX <= maxX && minY <= maxY;
    }

    void expand(const Point& p) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }

    void expand(const BoundingBox& other) {
        if (!other.isValid()) return;
        minX = std::min(minX, other.minX);
        minY = std::min(minY, other.minY);
        maxX = std::max(maxX, other.maxX);
        maxY = std::max(maxY, other.maxY);
    }

    bool contains(const Point& p, double eps = EPS) const {
        return p.x >= minX - eps && p.x <= maxX + eps &&
               p.y >= minY - eps && p.y <= maxY + eps;
    }

    bool contains(const BoundingBox& other, double eps = EPS) const {
        return other.minX >= minX - eps && other.maxX <= maxX + eps &&
               other.minY >= minY - eps && other.maxY <= maxY + eps;
    }

    bool intersects(const BoundingBox& other, double eps = EPS) const {
        return !(other.minX > maxX + eps || other.maxX < minX - eps ||
                 other.minY > maxY + eps || other.maxY < minY - eps);
    }
};

// Data-Oriented Polygon design: Contiguous memory for all points.
// Extremely beneficial for cache locality and MPI serialization.
struct Polygon {
    int id{};
    std::string name;
    BoundingBox bbox;

    // Single array holding all coordinates: 
    // The outer ring comes first, then all inner hole rings follow.
    std::vector<Point> vertices;

    // Defines the boundary logic.
    // ringOffsets[0] is always 0 (start of outer ring).
    // ringOffsets[1] is the end of the outer ring and start of the first hole.
    // ringOffsets.back() is always equal to vertices.size().
    std::vector<uint32_t> ringOffsets;
};

enum class Location {
    Outside,
    Boundary,
    Inside
};

double cross(const Point& a, const Point& b, const Point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

// Upgraded pointOnSegment explicitly accounts for valid point-to-line distance,
// not just raw uniform cross product (which scales inaccurately with line length)
bool pointOnSegment(const Point& p, const Point& a, const Point& b, double eps = EPS) {
    const double minX = std::min(a.x, b.x) - eps;
    const double maxX = std::max(a.x, b.x) + eps;
    const double minY = std::min(a.y, b.y) - eps;
    const double maxY = std::max(a.y, b.y) + eps;
    if (!(p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY)) return false;

    double pCross = std::fabs(cross(a, b, p));
    double segLenSq = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
    if (segLenSq < eps * eps) {
        // Line segment is effectively a single point
        return true;
    }
    
    // Real geometric perpendicular distance squared to the segment line
    return (pCross * pCross / segLenSq) <= (eps * eps);
}

BoundingBox computeBoundingBox(const Polygon& poly) {
    BoundingBox box;
    // We only need the outer ring for bbox, but reading all vertices is fast too
    for (const auto& p : poly.vertices) box.expand(p);
    return box;
}

void finalizePolygon(Polygon& poly) {
    poly.bbox = computeBoundingBox(poly);
}

// Ray-Casting algorithm over a contiguous array of points
Location pointInRing(const Point& p, const Point* ringPts, size_t n) {
    if (n < 3) return Location::Outside;

    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point& a = ringPts[j];
        const Point& b = ringPts[i];

        // 1. Point sits exactly on an edge or vertex
        if (pointOnSegment(p, a, b)) {
            return Location::Boundary;
        }

        // 2. Collinear vertex crossing checks
        // Using strict >= rules ensures we correctly evaluate if the ray passes 
        // horizontally through a vertex, preventing double counting errors.
        bool a_above = (a.y >= p.y);
        bool b_above = (b.y >= p.y);
        
        if (a_above != b_above) {
            double xIntersect = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (xIntersect > p.x) {
                inside = !inside;
            }
        }
    }
    return inside ? Location::Inside : Location::Outside;
}

Location pointInPolygon(const Point& p, const Polygon& poly) {
    if (!poly.bbox.contains(p)) return Location::Outside;

    if (poly.ringOffsets.size() < 2) return Location::Outside;

    // First evaluate against the outer ring
    size_t outerStart = poly.ringOffsets[0];
    size_t outerLen = poly.ringOffsets[1] - outerStart;
    const Point* outerPts = poly.vertices.data() + outerStart;

    Location outerLoc = pointInRing(p, outerPts, outerLen);
    
    // Fast fail
    if (outerLoc == Location::Outside) return Location::Outside;
    if (outerLoc == Location::Boundary) return Location::Boundary;

    // Check inner rings (holes)
    for (size_t r = 1; r < poly.ringOffsets.size() - 1; ++r) {
        size_t holeStart = poly.ringOffsets[r];
        size_t holeLen = poly.ringOffsets[r + 1] - holeStart;
        const Point* holePts = poly.vertices.data() + holeStart;

        Location holeLoc = pointInRing(p, holePts, holeLen);
        if (holeLoc == Location::Boundary) {
            return Location::Boundary; // touching hole boundary counts as polygon boundary
        }
        if (holeLoc == Location::Inside) {
            return Location::Outside; // strictly inside a hole means strictly outside the polygon
        }
    }

    return Location::Inside;
}

class Quadtree {
public:
    explicit Quadtree(BoundingBox boundary,
                      size_t capacity = 8,
                      int maxDepth = 12)
        : root_(std::move(boundary), capacity, maxDepth, 0) {}

    // Explicit clear guarantees no memory leaks
    ~Quadtree() {
        clear();
    }

    void clear() {
        root_.clear();
    }

    void build(const std::vector<Polygon>& regions) {
        for (const auto& region : regions) {
            root_.insert(&region);
        }
    }

    void query(const Point& p, std::vector<const Polygon*>& out) const {
        out.clear();
        root_.query(p, out);
    }

private:
    struct Node {
        BoundingBox boundary;
        size_t capacity;
        int maxDepth;
        int depth;
        bool divided{false};
        std::vector<const Polygon*> items;
        std::vector<std::unique_ptr<Node>> children;

        Node(BoundingBox box, size_t cap, int maxD, int dep)
            : boundary(std::move(box)), capacity(cap), maxDepth(maxD), depth(dep) {}

        void clear() {
            items.clear();
            children.clear(); // recursive safe cleanup due to unique_ptr RAII
            divided = false;
        }

        void subdivide() {
            if (divided) return;
            const double midX = (boundary.minX + boundary.maxX) * 0.5;
            const double midY = (boundary.minY + boundary.maxY) * 0.5;

            children.reserve(4);
            children.push_back(std::make_unique<Node>(BoundingBox{boundary.minX, midY, midX, boundary.maxY}, capacity, maxDepth, depth + 1));
            children.push_back(std::make_unique<Node>(BoundingBox{midX, midY, boundary.maxX, boundary.maxY}, capacity, maxDepth, depth + 1));
            children.push_back(std::make_unique<Node>(BoundingBox{boundary.minX, boundary.minY, midX, midY}, capacity, maxDepth, depth + 1));
            children.push_back(std::make_unique<Node>(BoundingBox{midX, boundary.minY, boundary.maxX, midY}, capacity, maxDepth, depth + 1));
            divided = true;
        }

        int childIndexIfFullyContained(const BoundingBox& box) const {
            if (!divided) return -1;
            for (int i = 0; i < 4; ++i) {
                if (children[i]->boundary.contains(box)) return i;
            }
            return -1;
        }

        void maybePushDownExistingItems() {
            if (!divided) return;
            std::vector<const Polygon*> remaining;
            remaining.reserve(items.size());
            for (const auto* item : items) {
                const int idx = childIndexIfFullyContained(item->bbox);
                if (idx >= 0) {
                    children[idx]->insert(item);
                } else {
                    remaining.push_back(item);
                }
            }
            items.swap(remaining);
        }

        bool insert(const Polygon* region) {
            if (!boundary.intersects(region->bbox)) return false;

            if (divided) {
                const int idx = childIndexIfFullyContained(region->bbox);
                if (idx >= 0) {
                    return children[idx]->insert(region);
                }
            }

            if (items.size() < capacity || depth >= maxDepth) {
                items.push_back(region);
                return true;
            }

            if (!divided) {
                subdivide();
                maybePushDownExistingItems();
            }

            const int idx = childIndexIfFullyContained(region->bbox);
            if (idx >= 0) {
                return children[idx]->insert(region);
            }

            items.push_back(region); // spanning region stays at current node
            return true;
        }

        void query(const Point& p, std::vector<const Polygon*>& out) const {
            if (!boundary.contains(p)) return;

            for (const auto* region : items) {
                if (region->bbox.contains(p)) out.push_back(region);
            }

            if (!divided) return;
            for (const auto& child : children) {
                if (child->boundary.contains(p)) {
                    child->query(p, out);
                }
            }
        }
    };

    Node root_;
};

std::vector<Point> generateUniformPoints(size_t n,
                                         const BoundingBox& domain,
                                         std::mt19937_64& rng) {
    std::uniform_real_distribution<double> dx(domain.minX, domain.maxX);
    std::uniform_real_distribution<double> dy(domain.minY, domain.maxY);
    std::vector<Point> points;
    points.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        points.push_back({dx(rng), dy(rng)});
    }
    return points;
}

std::vector<Point> generateClusteredPoints(size_t n,
                                           const BoundingBox& domain,
                                           std::mt19937_64& rng) {
    std::vector<Point> points;
    points.reserve(n);

    std::vector<Point> centers = {
        {25.0, 25.0}, {50.0, 50.0}, {75.0, 70.0}, {35.0, 80.0}
    };
    std::discrete_distribution<int> chooseCenter({35, 30, 20, 15});
    std::normal_distribution<double> noise(0.0, 6.0);
    std::uniform_real_distribution<double> mix(0.0, 1.0);
    std::uniform_real_distribution<double> ux(domain.minX, domain.maxX);
    std::uniform_real_distribution<double> uy(domain.minY, domain.maxY);

    auto clamp = [](double value, double lo, double hi) {
        return std::max(lo, std::min(value, hi));
    };

    for (size_t i = 0; i < n; ++i) {
        if (mix(rng) < 0.80) {
            const Point c = centers[chooseCenter(rng)];
            points.push_back({
                clamp(c.x + noise(rng), domain.minX, domain.maxX),
                clamp(c.y + noise(rng), domain.minY, domain.maxY)
            });
        } else {
            points.push_back({ux(rng), uy(rng)});
        }
    }
    return points;
}

std::vector<int> classifyPointBruteForce(const Point& p,
                                         const std::vector<Polygon>& regions) {
    std::vector<int> matches;
    for (const auto& region : regions) {
        const Location loc = pointInPolygon(p, region);
        if (loc != Location::Outside) {
            matches.push_back(region.id);
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

std::vector<int> classifyPointIndexed(const Point& p,
                                      const Quadtree& index,
                                      std::vector<const Polygon*>& scratch) {
    index.query(p, scratch);
    std::vector<int> matches;
    matches.reserve(scratch.size());
    std::unordered_set<int> seen;
    for (const auto* region : scratch) {
        if (!seen.insert(region->id).second) continue;
        const Location loc = pointInPolygon(p, *region);
        if (loc != Location::Outside) {
            matches.push_back(region->id);
        }
    }
    std::sort(matches.begin(), matches.end());
    return matches;
}

struct BenchmarkStats {
    double bruteForceSeconds{};
    double quadtreeSeconds{};
    double averageCandidates{};
    size_t totalMatches{};
};

BenchmarkStats benchmarkDataset(const std::vector<Point>& points,
                                const std::vector<Polygon>& regions,
                                const Quadtree& index,
                                bool verifyExact) {
    using clock = std::chrono::high_resolution_clock;

    size_t totalMatchesBF = 0;
    auto startBF = clock::now();
    std::vector<std::vector<int>> bruteResults;
    if (verifyExact) bruteResults.reserve(points.size());

    for (const auto& p : points) {
        auto result = classifyPointBruteForce(p, regions);
        totalMatchesBF += result.size();
        if (verifyExact) bruteResults.push_back(std::move(result));
    }
    auto endBF = clock::now();

    size_t totalMatchesQT = 0;
    size_t totalCandidates = 0;
    std::vector<const Polygon*> scratch;
    auto startQT = clock::now();
    for (size_t i = 0; i < points.size(); ++i) {
        index.query(points[i], scratch);
        totalCandidates += scratch.size();
        auto result = classifyPointIndexed(points[i], index, scratch);
        totalMatchesQT += result.size();
        if (verifyExact && result != bruteResults[i]) {
            std::ostringstream oss;
            oss << "Mismatch at point (" << points[i].x << ", " << points[i].y << ")";
            throw std::runtime_error(oss.str());
        }
    }
    auto endQT = clock::now();

    if (totalMatchesBF != totalMatchesQT) {
        throw std::runtime_error("Mismatch in total match counts between brute force and quadtree.");
    }

    BenchmarkStats stats;
    stats.bruteForceSeconds = std::chrono::duration<double>(endBF - startBF).count();
    stats.quadtreeSeconds = std::chrono::duration<double>(endQT - startQT).count();
    stats.averageCandidates = points.empty() ? 0.0 : static_cast<double>(totalCandidates) / static_cast<double>(points.size());
    stats.totalMatches = totalMatchesQT;
    return stats;
}

Polygon makeSimpleRectanglePolygon(int id, const std::string& name,
                                        double minX, double minY, double maxX, double maxY) {
    Polygon poly;
    poly.id = id;
    poly.name = name;
    poly.ringOffsets.push_back(0); // Outer ring starts
    poly.vertices.push_back({minX, minY});
    poly.vertices.push_back({maxX, minY});
    poly.vertices.push_back({maxX, maxY});
    poly.vertices.push_back({minX, maxY});
    poly.ringOffsets.push_back(4); // Outer ring ends / Total points
    finalizePolygon(poly);
    return poly;
}

std::vector<Polygon> buildCorrectnessDataset() {
    std::vector<Polygon> regions;

    // Region 1: simple square
    regions.push_back(makeSimpleRectanglePolygon(1, "Square-A", 10.0, 10.0, 30.0, 30.0));

    // Region 2: donut polygon with an interior hole
    Polygon donut;
    donut.id = 2;
    donut.name = "Donut-Zone";
    donut.ringOffsets.push_back(0);
    // Outer
    donut.vertices.push_back({40.0, 40.0});
    donut.vertices.push_back({80.0, 40.0});
    donut.vertices.push_back({80.0, 80.0});
    donut.vertices.push_back({40.0, 80.0});
    donut.ringOffsets.push_back(static_cast<uint32_t>(donut.vertices.size()));
    
    // Hole (reversing order doesn't impact parity ray-casting, but it's mathematically sound)
    donut.vertices.push_back({50.0, 50.0});
    donut.vertices.push_back({70.0, 50.0});
    donut.vertices.push_back({70.0, 70.0});
    donut.vertices.push_back({50.0, 70.0});
    donut.ringOffsets.push_back(static_cast<uint32_t>(donut.vertices.size()));
    finalizePolygon(donut);
    regions.push_back(donut);

    // Region 3: Overlapping square to test multiple memberships
    regions.push_back(makeSimpleRectanglePolygon(3, "Overlap-Zone", 25.0, 25.0, 45.0, 45.0));

    // Region 4: Testing ray collinearity exactly along top edge
    Polygon collinear;
    collinear.id = 4;
    collinear.name = "Collinear-Edge";
    collinear.ringOffsets.push_back(0);
    collinear.vertices.push_back({10.0, 90.0});
    collinear.vertices.push_back({30.0, 90.0});
    collinear.vertices.push_back({30.0, 95.0});
    collinear.vertices.push_back({10.0, 95.0});
    collinear.ringOffsets.push_back(4);
    finalizePolygon(collinear);
    regions.push_back(collinear);

    return regions;
}

std::vector<Polygon> buildBenchmarkDataset(int gridX, int gridY) {
    std::vector<Polygon> regions;
    int id = 1000;

    const double stepX = 100.0 / static_cast<double>(gridX);
    const double stepY = 100.0 / static_cast<double>(gridY);

    for (int ix = 0; ix < gridX; ++ix) {
        for (int iy = 0; iy < gridY; ++iy) {
            const double minX = ix * stepX;
            const double minY = iy * stepY;
            const double maxX = minX + stepX * 0.9;
            const double maxY = minY + stepY * 0.9;

            Polygon poly;
            poly.id = id++;
            poly.name = "Grid-Cell-" + std::to_string(poly.id);
            
            // Outer ring
            poly.ringOffsets.push_back(0);
            poly.vertices.push_back({minX, minY});
            poly.vertices.push_back({maxX, minY});
            poly.vertices.push_back({maxX, maxY});
            poly.vertices.push_back({minX, maxY});
            
            // Create a complex hole structure in every grid polygon to fulfill benchmark demands
            const double midX = (minX + maxX) * 0.5;
            const double midY = (minY + maxY) * 0.5;
            const double hdx = (maxX - minX) * 0.2;
            const double hdy = (maxY - minY) * 0.2;
            
            poly.ringOffsets.push_back(static_cast<uint32_t>(poly.vertices.size())); // close outer
            
            // inner ring (hole)
            poly.vertices.push_back({midX - hdx, midY - hdy});
            poly.vertices.push_back({midX + hdx, midY - hdy});
            poly.vertices.push_back({midX + hdx, midY + hdy});
            poly.vertices.push_back({midX - hdx, midY + hdy});

            poly.ringOffsets.push_back(static_cast<uint32_t>(poly.vertices.size())); // close hole
            
            finalizePolygon(poly);
            regions.push_back(std::move(poly));
        }
    }

    return regions;
}

std::string vecToString(const std::vector<int>& v) {
    std::ostringstream oss;
    oss << "{";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << ", ";
        oss << v[i];
    }
    oss << "}";
    return oss.str();
}

void runDeterministicCorrectnessTests(const std::vector<Polygon>& regions) {
    const BoundingBox domain{0.0, 0.0, 100.0, 100.0};
    Quadtree index(domain, 6, 10);
    index.build(regions);
    std::vector<const Polygon*> scratch;

    struct TestCase {
        Point p;
        std::vector<int> expected;
        std::string label;
    };

    const std::vector<TestCase> tests = {
        {{10, 10}, {1}, "square vertex"},
        {{20, 20}, {1}, "square interior"},
        {{30, 30}, {1, 3}, "shared overlap boundary"},
        {{35, 35}, {3}, "overlap-only interior"},
        {{60, 60}, {}, "inside hole => outside polygon"},
        {{50, 60}, {2}, "hole boundary (included as boundary)"},
        {{45, 45}, {2, 3}, "donut outer boundary and overlap corner"},
        {{20, 90}, {4}, "collinear exact edge match"},
        {{90, 90}, {}, "outside all"}
    };

    for (const auto& test : tests) {
        const auto brute = classifyPointBruteForce(test.p, regions);
        const auto indexed = classifyPointIndexed(test.p, index, scratch);
        if (brute != test.expected || indexed != test.expected) {
            std::cerr << "Deterministic test failed: " << test.label << "\n"
                      << "Expected: " << vecToString(test.expected) << "\n"
                      << "Brute:    " << vecToString(brute) << "\n"
                      << "Indexed:  " << vecToString(indexed) << "\n";
            throw std::runtime_error("Deterministic correctness suite failed.");
        }
    }
}

void printBenchmarkLine(const std::string& label, const BenchmarkStats& stats) {
    std::cout << std::left << std::setw(18) << label
              << " | Brute Force: " << std::setw(8) << std::fixed << std::setprecision(4) << std::max(0.0001, stats.bruteForceSeconds) << " s"
              << " | Quadtree: " << std::setw(8) << std::max(0.0001, stats.quadtreeSeconds) << " s"
              << " | Avg candidates: " << std::setw(5) << std::setprecision(2) << stats.averageCandidates
              << " | Total matches: " << stats.totalMatches
              << '\n';
}

} // namespace pip

int main() {
    using namespace pip;

    try {
        std::cout << "=== Milestone 1: Sequential Baseline with Robust Spatial Indexing ===\n\n";

        const BoundingBox domain{0.0, 0.0, 100.0, 100.0};

        // 1) Correctness dataset: edges, vertices, holes, overlaps
        auto correctnessRegions = buildCorrectnessDataset();
        runDeterministicCorrectnessTests(correctnessRegions);
        std::cout << "[OK] Deterministic correctness tests passed (edges, vertices, collinear vertices, holes, overlaps).\n";

        // 2) Random validation: exact brute-force vs quadtree agreement
        Quadtree correctnessIndex(domain, 6, 10);
        correctnessIndex.build(correctnessRegions);

        std::mt19937_64 rng(42);
        auto uniformValidation = generateUniformPoints(50000, domain, rng);
        auto clusteredValidation = generateClusteredPoints(50000, domain, rng);

        const auto uniformValidationStats = benchmarkDataset(uniformValidation, correctnessRegions, correctnessIndex, true);
        const auto clusteredValidationStats = benchmarkDataset(clusteredValidation, correctnessRegions, correctnessIndex, true);
        std::cout << "[OK] Random validation passed for uniform and clustered GPS datasets.\n\n";

        std::cout << "Validation summary:\n";
        printBenchmarkLine("Uniform (50k)", uniformValidationStats);
        printBenchmarkLine("Clustered (50k)", clusteredValidationStats);
        std::cout << '\n';

        // 3) Baseline benchmark with 500+ complex regions and 100k+ points
        // Grid 25x20 = 500 complex polygons (each with inner hole logic)
        auto benchmarkRegions = buildBenchmarkDataset(25, 20); 
        Quadtree benchmarkIndex(domain, 8, 12);
        benchmarkIndex.build(benchmarkRegions);

        auto uniformBenchmark = generateUniformPoints(100000, domain, rng);
        auto clusteredBenchmark = generateClusteredPoints(100000, domain, rng);

        const auto uniformStats = benchmarkDataset(uniformBenchmark, benchmarkRegions, benchmarkIndex, true);
        const auto clusteredStats = benchmarkDataset(clusteredBenchmark, benchmarkRegions, benchmarkIndex, true);

        std::cout << "Baseline benchmark (500 Complex Polygons with Holes, 100K Points):\n";
        printBenchmarkLine("Uniform   (100k)", uniformStats);
        printBenchmarkLine("Clustered (100k)", clusteredStats);

        std::cout << "\nMilestone 1 implementation complete: robust ray-casting, data-oriented holes struct, quadtree MBB filtering, memory cleanup, synthetic GPS generation, and brute-force verification.\n";
    } catch (const std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
