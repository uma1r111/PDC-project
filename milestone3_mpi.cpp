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
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <fstream>
#include <cstring>
#include <omp.h>
#include <mpi.h>

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

struct Polygon {
    int id{};
    std::string name;
    BoundingBox bbox;
    std::vector<Point> vertices;
    std::vector<uint32_t> ringOffsets;
    std::vector<uint32_t> partOffsets; 
};

enum class Location {
    Outside,
    Boundary,
    Inside
};

double cross(const Point& a, const Point& b, const Point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool pointOnSegment(const Point& p, const Point& a, const Point& b, double eps = EPS) {
    const double minX = std::min(a.x, b.x) - eps;
    const double maxX = std::max(a.x, b.x) + eps;
    const double minY = std::min(a.y, b.y) - eps;
    const double maxY = std::max(a.y, b.y) + eps;
    if (!(p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY)) return false;

    double pCross = std::fabs(cross(a, b, p));
    double segLenSq = (b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y);
    if (segLenSq < eps * eps) {
        return true;
    }
    return (pCross * pCross / segLenSq) <= (eps * eps);
}

BoundingBox computeBoundingBox(const Polygon& poly) {
    BoundingBox box;
    for (const auto& p : poly.vertices) box.expand(p);
    return box;
}

void finalizePolygon(Polygon& poly) {
    poly.bbox = computeBoundingBox(poly);
}

Location pointInRing(const Point& p, const Point* ringPts, size_t n) {
    if (n < 3) return Location::Outside;

    bool inside = false;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Point& a = ringPts[j];
        const Point& b = ringPts[i];

        if (pointOnSegment(p, a, b)) {
            return Location::Boundary;
        }

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

    size_t outerStart = poly.ringOffsets[0];
    size_t outerLen = poly.ringOffsets[1] - outerStart;
    const Point* outerPts = poly.vertices.data() + outerStart;

    Location outerLoc = pointInRing(p, outerPts, outerLen);

    if (outerLoc == Location::Outside) return Location::Outside;
    if (outerLoc == Location::Boundary) return Location::Boundary;

    for (size_t r = 1; r < poly.ringOffsets.size() - 1; ++r) {
        size_t holeStart = poly.ringOffsets[r];
        size_t holeLen = poly.ringOffsets[r + 1] - holeStart;
        const Point* holePts = poly.vertices.data() + holeStart;

        Location holeLoc = pointInRing(p, holePts, holeLen);
        if (holeLoc == Location::Boundary) return Location::Boundary;
        if (holeLoc == Location::Inside) return Location::Outside;
    }

    return Location::Inside;
}

class Quadtree {
public:
    explicit Quadtree(BoundingBox boundary, size_t capacity = 8, int maxDepth = 12)
        : root_(std::move(boundary), capacity, maxDepth, 0) {}

    ~Quadtree() { clear(); }
    void clear() { root_.clear(); }

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
            children.clear();
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

            items.push_back(region);
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

// Custom MPI Serialization
void packInt(std::vector<char>& buf, int val) {
    const char* ptr = reinterpret_cast<const char*>(&val);
    buf.insert(buf.end(), ptr, ptr + sizeof(int));
}
void packDouble(std::vector<char>& buf, double val) {
    const char* ptr = reinterpret_cast<const char*>(&val);
    buf.insert(buf.end(), ptr, ptr + sizeof(double));
}
void packUint32(std::vector<char>& buf, uint32_t val) {
    const char* ptr = reinterpret_cast<const char*>(&val);
    buf.insert(buf.end(), ptr, ptr + sizeof(uint32_t));
}
void packString(std::vector<char>& buf, const std::string& str) {
    packInt(buf, str.size());
    buf.insert(buf.end(), str.begin(), str.end());
}
int unpackInt(const char*& ptr) {
    int val; std::memcpy(&val, ptr, sizeof(int)); ptr += sizeof(int); return val;
}
double unpackDouble(const char*& ptr) {
    double val; std::memcpy(&val, ptr, sizeof(double)); ptr += sizeof(double); return val;
}
uint32_t unpackUint32(const char*& ptr) {
    uint32_t val; std::memcpy(&val, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t); return val;
}
std::string unpackString(const char*& ptr) {
    int len = unpackInt(ptr);
    std::string str(ptr, len); ptr += len; return str;
}

std::vector<char> serializePolygons(const std::vector<Polygon>& polys) {
    std::vector<char> buf;
    packInt(buf, polys.size());
    for(const auto& p : polys) {
        packInt(buf, p.id);
        packString(buf, p.name);
        packDouble(buf, p.bbox.minX); packDouble(buf, p.bbox.minY);
        packDouble(buf, p.bbox.maxX); packDouble(buf, p.bbox.maxY);
        packInt(buf, p.vertices.size());
        for(const auto& v : p.vertices) { packDouble(buf, v.x); packDouble(buf, v.y); }
        packInt(buf, p.ringOffsets.size());
        for(auto r : p.ringOffsets) packUint32(buf, r);
        packInt(buf, p.partOffsets.size());
        for(auto part : p.partOffsets) packUint32(buf, part);
    }
    return buf;
}

std::vector<Polygon> deserializePolygons(const std::vector<char>& buf) {
    std::vector<Polygon> polys;
    const char* ptr = buf.data();
    int count = unpackInt(ptr);
    polys.resize(count);
    for(int i=0; i<count; ++i) {
        polys[i].id = unpackInt(ptr);
        polys[i].name = unpackString(ptr);
        polys[i].bbox.minX = unpackDouble(ptr); polys[i].bbox.minY = unpackDouble(ptr);
        polys[i].bbox.maxX = unpackDouble(ptr); polys[i].bbox.maxY = unpackDouble(ptr);
        int numVerts = unpackInt(ptr);
        polys[i].vertices.resize(numVerts);
        for(int j=0; j<numVerts; ++j) { polys[i].vertices[j].x = unpackDouble(ptr); polys[i].vertices[j].y = unpackDouble(ptr); }
        int numRings = unpackInt(ptr);
        polys[i].ringOffsets.resize(numRings);
        for(int j=0; j<numRings; ++j) polys[i].ringOffsets[j] = unpackUint32(ptr);
        int numParts = unpackInt(ptr);
        polys[i].partOffsets.resize(numParts);
        for(int j=0; j<numParts; ++j) polys[i].partOffsets[j] = unpackUint32(ptr);
    }
    return polys;
}

std::vector<Point> generateClusteredPoints(size_t n, const BoundingBox& domain, std::mt19937_64& rng) {
    std::vector<Point> points;
    points.reserve(n);
    std::vector<Point> centers = {{25.0, 25.0}, {50.0, 50.0}, {75.0, 70.0}, {35.0, 80.0}};
    std::discrete_distribution<int> chooseCenter({35, 30, 20, 15});
    std::normal_distribution<double> noise(0.0, 6.0);
    std::uniform_real_distribution<double> mix(0.0, 1.0);
    std::uniform_real_distribution<double> ux(domain.minX, domain.maxX);
    std::uniform_real_distribution<double> uy(domain.minY, domain.maxY);

    auto clamp = [](double value, double lo, double hi) { return std::max(lo, std::min(value, hi)); };

    for (size_t i = 0; i < n; ++i) {
        if (mix(rng) < 0.80) {
            const Point c = centers[chooseCenter(rng)];
            points.push_back({clamp(c.x + noise(rng), domain.minX, domain.maxX), clamp(c.y + noise(rng), domain.minY, domain.maxY)});
        } else {
            points.push_back({ux(rng), uy(rng)});
        }
    }
    return points;
}

std::vector<int> classifyPointIndexed(const Point& p, const Quadtree& index, std::vector<const Polygon*>& scratch) {
    index.query(p, scratch);
    std::vector<int> matches;
    matches.reserve(scratch.size());
    for (const auto* region : scratch) {
        if (pointInPolygon(p, *region) != Location::Outside) {
            matches.push_back(region->id);
        }
    }
    if (!matches.empty()) {
        std::sort(matches.begin(), matches.end());
        matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    }
    return matches;
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

            poly.ringOffsets.push_back(0);
            poly.vertices.push_back({minX, minY});
            poly.vertices.push_back({maxX, minY});
            poly.vertices.push_back({maxX, maxY});
            poly.vertices.push_back({minX, maxY});

            const double midX = (minX + maxX) * 0.5;
            const double midY = (minY + maxY) * 0.5;
            const double hdx = (maxX - minX) * 0.2;
            const double hdy = (maxY - minY) * 0.2;

            poly.ringOffsets.push_back(static_cast<uint32_t>(poly.vertices.size()));
            poly.vertices.push_back({midX - hdx, midY - hdy});
            poly.vertices.push_back({midX + hdx, midY - hdy});
            poly.vertices.push_back({midX + hdx, midY + hdy});
            poly.vertices.push_back({midX - hdx, midY + hdy});
            poly.ringOffsets.push_back(static_cast<uint32_t>(poly.vertices.size()));
            
            // Populate dummy partOffsets for consistency
            poly.partOffsets.push_back(0);

            finalizePolygon(poly);
            regions.push_back(std::move(poly));
        }
    }
    return regions;
}

void runReplicateMode(int rank, int size, size_t numPoints, int threads, const std::vector<Polygon>& allPolys, const BoundingBox& domain) {
    std::vector<char> polyBuf;
    int bufSize = 0;
    if(rank == 0) {
        polyBuf = serializePolygons(allPolys);
        bufSize = polyBuf.size();
    }
    MPI_Bcast(&bufSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if(rank != 0) polyBuf.resize(bufSize);
    MPI_Bcast(polyBuf.data(), bufSize, MPI_CHAR, 0, MPI_COMM_WORLD);
    
    std::vector<Polygon> localPolys = (rank == 0) ? allPolys : deserializePolygons(polyBuf);
    Quadtree index(domain, 8, 12);
    index.build(localPolys);

    std::vector<Point> allPoints;
    std::vector<int> sendCounts(size, 0);
    std::vector<int> displs(size, 0);
    
    if(rank == 0) {
        std::mt19937_64 rng(42);
        allPoints = generateClusteredPoints(numPoints, domain, rng);
        int base = numPoints / size;
        int rem = numPoints % size;
        int currDisp = 0;
        for(int i = 0; i < size; ++i) {
            sendCounts[i] = (base + (i < rem ? 1 : 0)) * sizeof(Point);
            displs[i] = currDisp;
            currDisp += sendCounts[i];
        }
    }
    
    int localPointBytes = 0;
    MPI_Scatter(sendCounts.data(), 1, MPI_INT, &localPointBytes, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    std::vector<Point> localPoints(localPointBytes / sizeof(Point));
    MPI_Scatterv(allPoints.data(), sendCounts.data(), displs.data(), MPI_BYTE,
                 localPoints.data(), localPointBytes, MPI_BYTE, 0, MPI_COMM_WORLD);

    size_t localMatches = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    double tStart = MPI_Wtime();
    
    #pragma omp parallel num_threads(threads) reduction(+:localMatches)
    {
        std::vector<const Polygon*> scratch;
        #pragma omp for schedule(dynamic, 256)
        for(size_t i = 0; i < localPoints.size(); ++i) {
            auto matches = classifyPointIndexed(localPoints[i], index, scratch);
            localMatches += matches.size();
        }
    }
    
    double tEnd = MPI_Wtime();
    double localTime = tEnd - tStart;
    double maxTime = 0.0;
    MPI_Reduce(&localTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    unsigned long long lMatches = localMatches, gMatches = 0;
    MPI_Reduce(&lMatches, &gMatches, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    
    if(rank == 0) {
        std::cout << "Replicate Mode completed in " << maxTime << " s, matches: " << gMatches << "\n";
        std::ofstream ofs("benchmark_metrics.csv", std::ios::app);
        if (ofs.tellp() == 0) {
            ofs << "strategy,num_nodes,omp_threads,total_points,exec_time_s,matches\n";
        }
        ofs << "replicate," << size << "," << threads << "," << numPoints << "," << maxTime << "," << gMatches << "\n";
    }
}

void runShardMode(int rank, int size, size_t numPoints, int threads, const std::vector<Polygon>& allPolys, const BoundingBox& domain) {
    int gridX = std::max(1, (int)std::sqrt(size));
    int gridY = size / gridX;
    while(gridX * gridY != size) {
        gridX++;
        gridY = size / gridX;
    } 
    
    double stepX = (domain.maxX - domain.minX) / gridX;
    double stepY = (domain.maxY - domain.minY) / gridY;
    
    int myX = rank % gridX;
    int myY = rank / gridX;
    BoundingBox myBox;
    myBox.minX = domain.minX + myX * stepX;
    myBox.maxX = myBox.minX + stepX;
    myBox.minY = domain.minY + myY * stepY;
    myBox.maxY = myBox.minY + stepY;

    std::vector<Polygon> localPolys;
    std::vector<char> polyBuf;
    int bufSize = 0;
    if(rank == 0) {
        std::ofstream ofs("domain_decomposition.csv");
        ofs << "polygon_id,polygon_name,assigned_rank,bbox_minX,bbox_minY,bbox_maxX,bbox_maxY\n";
        
        std::vector<std::vector<Polygon>> rankPolys(size);
        for(const auto& p : allPolys) {
            for(int r = 0; r < size; ++r) {
                int rx = r % gridX;
                int ry = r / gridX;
                BoundingBox rBox{domain.minX + rx*stepX, domain.minY + ry*stepY, domain.minX + (rx+1)*stepX, domain.minY + (ry+1)*stepY};
                if(rBox.intersects(p.bbox)) {
                    rankPolys[r].push_back(p);
                    ofs << p.id << "," << p.name << "," << r << "," << p.bbox.minX << "," << p.bbox.minY << "," << p.bbox.maxX << "," << p.bbox.maxY << "\n";
                }
            }
        }
        
        for(int r = 1; r < size; ++r) {
            auto buf = serializePolygons(rankPolys[r]);
            int sz = buf.size();
            MPI_Send(&sz, 1, MPI_INT, r, 0, MPI_COMM_WORLD);
            MPI_Send(buf.data(), sz, MPI_CHAR, r, 1, MPI_COMM_WORLD);
        }
        localPolys = std::move(rankPolys[0]);
    } else {
        MPI_Recv(&bufSize, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        polyBuf.resize(bufSize);
        MPI_Recv(polyBuf.data(), bufSize, MPI_CHAR, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        localPolys = deserializePolygons(polyBuf);
    }
    
    Quadtree index(myBox, 8, 12);
    index.build(localPolys);

    std::vector<Point> myPoints;
    std::vector<int> sendCounts(size, 0);
    std::vector<Point> sendBuffer;
    std::vector<int> sdispls(size, 0);
    
    if(rank == 0) {
        std::mt19937_64 rng(42);
        auto allPoints = generateClusteredPoints(numPoints, domain, rng);
        
        std::vector<int> density(32 * 32, 0);
        for(const auto& p : allPoints) {
            int cx = std::max(0, std::min((int)((p.x - domain.minX)/(domain.maxX - domain.minX)*32), 31));
            int cy = std::max(0, std::min((int)((p.y - domain.minY)/(domain.maxY - domain.minY)*32), 31));
            density[cy * 32 + cx]++;
        }
        std::ofstream ofs("spatial_skew.csv");
        for(int cy = 0; cy < 32; ++cy) {
            for(int cx = 0; cx < 32; ++cx) {
                ofs << density[cy*32 + cx] << (cx == 31 ? "" : ",");
            }
            ofs << "\n";
        }
        
        std::vector<std::vector<Point>> rankPoints(size);
        for(const auto& p : allPoints) {
            int cx = std::max(0, std::min((int)((p.x - domain.minX)/(domain.maxX - domain.minX)*gridX), gridX-1));
            int cy = std::max(0, std::min((int)((p.y - domain.minY)/(domain.maxY - domain.minY)*gridY), gridY-1));
            int targetRank = cy * gridX + cx;
            rankPoints[targetRank].push_back(p);
        }
        
        int currDisp = 0;
        for(int i = 0; i < size; ++i) {
            sendCounts[i] = rankPoints[i].size() * sizeof(Point);
            sdispls[i] = currDisp;
            currDisp += sendCounts[i];
            sendBuffer.insert(sendBuffer.end(), rankPoints[i].begin(), rankPoints[i].end());
        }
    }
    
    int recvCount = 0;
    MPI_Scatter(sendCounts.data(), 1, MPI_INT, &recvCount, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    myPoints.resize(recvCount / sizeof(Point));
    MPI_Scatterv(sendBuffer.data(), sendCounts.data(), sdispls.data(), MPI_BYTE,
                 myPoints.data(), recvCount, MPI_BYTE, 0, MPI_COMM_WORLD);
                 
    size_t localMatches = 0;
    MPI_Barrier(MPI_COMM_WORLD);
    double tStart = MPI_Wtime();
    
    #pragma omp parallel num_threads(threads) reduction(+:localMatches)
    {
        std::vector<const Polygon*> scratch;
        #pragma omp for schedule(dynamic, 256)
        for(size_t i = 0; i < myPoints.size(); ++i) {
            auto matches = classifyPointIndexed(myPoints[i], index, scratch);
            localMatches += matches.size();
        }
    }
    
    double tEnd = MPI_Wtime();
    double localTime = tEnd - tStart;
    double maxTime = 0.0;
    MPI_Reduce(&localTime, &maxTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    
    unsigned long long lMatches = localMatches, gMatches = 0;
    MPI_Reduce(&lMatches, &gMatches, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    
    if(rank == 0) {
        std::cout << "Shard Mode completed in " << maxTime << " s, matches: " << gMatches << "\n";
        std::ofstream ofs("benchmark_metrics.csv", std::ios::app);
        if (ofs.tellp() == 0) {
            ofs << "strategy,num_nodes,omp_threads,total_points,exec_time_s,matches\n";
        }
        ofs << "shard," << size << "," << threads << "," << numPoints << "," << maxTime << "," << gMatches << "\n";
    }
}

} // namespace pip

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    std::string mode = "replicate";
    size_t numPoints = 10000000;
    int threads = 4;
    
    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg.find("--mode=") == 0) mode = arg.substr(7);
        else if(arg.find("--points=") == 0) numPoints = std::stoull(arg.substr(9));
        else if(arg.find("--omp-threads=") == 0) threads = std::stoi(arg.substr(14));
    }
    
    if(rank == 0) {
        std::cout << "=== Milestone 3: MPI + OpenMP Hybrid ===\n";
        std::cout << "Nodes: " << size << " | Threads/Node: " << threads << " | Points: " << numPoints << " | Mode: " << mode << "\n";
    }
    
    pip::BoundingBox domain{0.0, 0.0, 100.0, 100.0};
    std::vector<pip::Polygon> allPolys;
    if(rank == 0) {
        allPolys = pip::buildBenchmarkDataset(50, 40); // 2000 complex polygons
    }
    
    if(mode == "replicate") {
        pip::runReplicateMode(rank, size, numPoints, threads, allPolys, domain);
    } else if(mode == "shard") {
        pip::runShardMode(rank, size, numPoints, threads, allPolys, domain);
    } else {
        if(rank == 0) std::cerr << "Unknown mode: " << mode << "\n";
    }
    
    MPI_Finalize();
    return 0;
}
