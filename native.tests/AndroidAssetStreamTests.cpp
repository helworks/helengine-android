#include "../src/platform/android/runtime/AndroidAssetContentStreamSource.hpp"
#include "../src/platform/android/runtime/AndroidAssetStream.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

/// Exercises the real buffered asset stream and content source without an Android device or filesystem extraction.
class AndroidAssetStreamTests {
public:
    /// Runs independent observable stream and content-source scenarios.
    static void Run() {
        BufferedBytes(); BulkAndShortReads(); SeekAndBuffer(); IndependentReaders(); ReadOnlyAndLifetime();
        PathsAndMissingAssets(); FailuresAndTruncation(); EmptyAndBounds();
        std::cout<<"8 Android asset-stream scenarios passed.\n";
    }
private:
    /// Throws descriptive failures even when release assertions are disabled.
    static void Check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
    /// Requires an operation to reject invalid input or native failures explicitly.
    static void Throws(const std::function<void()>& operation,const char* message) {
        bool failed=false; try { operation(); } catch (const std::exception&) { failed=true; } Check(failed,message);
    }
    /// Builds deterministic fake APK bytes large enough to cross buffer boundaries.
    static AAssetManager Manager(size_t length=50000) {
        AAssetManager manager{}; auto& bytes=manager.Files["cooked/test.bin"]; bytes.resize(length);
        for (size_t index=0;index<length;++index) bytes[index]=static_cast<uint8_t>(index%251);
        return manager;
    }
    /// Thousands of deserializer byte reads use a bounded number of native calls and correctly report EOF.
    static void BufferedBytes() {
        auto manager=Manager(); helengine::android::AndroidAssetContentStreamSource source(&manager);
        { std::unique_ptr<Stream> stream(source.OpenRead("cooked/test.bin"));
            for (size_t index=0;index<50000;++index) Check(stream->InternalReadByte()==static_cast<int>(index%251),"Buffered bytes must retain exact asset contents.");
            Check(stream->Position()==50000 && stream->InternalReadByte()==-1 && manager.Reads==4,"Byte reads must buffer SDK access and report real EOF.");
        }
        Check(manager.Handles==0,"Deserializer ownership must close the handle.");
    }
    /// Bulk reads preserve buffer offsets, consume existing cached bytes and handle SDK short reads.
    static void BulkAndShortReads() {
        auto manager=Manager(); manager.ReadLimit=777;
        helengine::android::AndroidAssetStream stream(&manager,"cooked/test.bin"); Check(stream.InternalReadByte()==0,"First read must fill the cache.");
        std::vector<uint8_t> bytes(50010,0xff);
        Check(stream.Read(bytes.data(),5,50000)==49999,"Read must stop at the declared EOF after a cached byte.");
        for (size_t index=0;index<49999;++index) Check(bytes[index+5]==static_cast<uint8_t>((index+1)%251),"Short native reads must preserve every requested byte.");
        Check(bytes[4]==0xff && bytes[50004]==0xff && stream.Position()==50000,"Read destination boundaries and logical position must be exact.");
    }
    /// Cached backward/forward seeks avoid native work while out-of-cache seeks preserve all origins.
    static void SeekAndBuffer() {
        auto manager=Manager(); helengine::android::AndroidAssetStream stream(&manager,"cooked/test.bin");
        Check(stream.InternalReadByte()==0,"Initial byte must fill the cache.");
        stream.SetPosition(10); Check(stream.InternalReadByte()==10 && manager.Seeks==0,"Seek inside the cache must reuse buffered bytes.");
        Check(stream.Seek(-5,Current)==6 && stream.InternalReadByte()==6,"Current seek must use consumer position, not SDK read-ahead.");
        Check(stream.Seek(-2,End)==49998 && stream.InternalReadByte()==static_cast<int>(49998%251) && manager.Seeks==1,"End-relative seek must reach the real uncompressed end.");
        stream.Seek(0,Begin); Check(stream.InternalReadByte()==0 && manager.Seeks==2,"Out-of-cache backward seek must reposition the native handle.");
        stream.Seek(16384,Begin); Check(stream.InternalReadByte()==static_cast<int>(16384%251),"Seek to a cache boundary must not use stale bytes.");
    }
    /// Concurrent readers share only the asset manager; each open retains an independent cursor and handle.
    static void IndependentReaders() {
        auto manager=Manager(); helengine::android::AndroidAssetContentStreamSource source(&manager);
        std::unique_ptr<Stream> first(source.OpenRead("cooked/test.bin")); std::unique_ptr<Stream> second(source.OpenRead("./cooked/./test.bin"));
        first->SetPosition(40000); Check(first->InternalReadByte()==static_cast<int>(40000%251) && second->Position()==0 && second->InternalReadByte()==0 && manager.Handles==2,"Separate opens must not share cursor state.");
        first.reset(); Check(manager.Handles==1 && second->InternalReadByte()==1,"Closing one reader must leave another usable.");
    }
    /// Packaged content rejects mutations and closes exactly once through explicit or automatic lifetime paths.
    static void ReadOnlyAndLifetime() {
        auto manager=Manager();
        { helengine::android::AndroidAssetStream stream(&manager,"cooked/test.bin"); uint8_t byte=0;
            Check(stream.CanRead() && stream.CanSeek() && !stream.CanWrite(),"Open stream capabilities must match read-only APK assets.");
            Throws([&] { stream.Write(&byte,0,1); },"Write must fail."); Throws([&] { stream.SetLength(100); },"Resize must fail.");
            Throws([&] { stream.InternalReserve(10); },"Write reservation must fail."); Throws([&] { stream.InternalWriteByte(1); },"Byte write must fail.");
            stream.Flush(); stream.Dispose(); stream.Close(); Check(!stream.CanRead() && !stream.CanSeek() && manager.Handles==0,"Close must be idempotent.");
            Throws([&] { stream.Read(&byte,0,1); },"Reading a closed stream must fail."); Throws([&] { stream.Length(); },"Closed metadata access must fail.");
        }
        Check(manager.Handles==0,"Destructor after disposal must not double close.");
    }
    /// Paths cannot escape cooked content, and missing assets never become empty default streams.
    static void PathsAndMissingAssets() {
        auto manager=Manager(); helengine::android::AndroidAssetContentStreamSource source(&manager);
        for (const auto& path:{"","/cooked/test.bin","../cooked/test.bin","cooked/../test.bin","C:/cooked/test.bin","cooked\\test.bin","shaders/test.bin","cooked/"}) {
            Throws([&] { std::unique_ptr<Stream> stream(source.OpenRead(path)); },"Invalid content path must be rejected.");
        }
        Throws([&] { std::unique_ptr<Stream> stream(source.OpenRead(std::string("cooked/test.bin\0ignored",23))); },"Embedded null path must fail.");
        Throws([&] { std::unique_ptr<Stream> stream(source.OpenRead("cooked/missing.bin")); },"Missing asset must fail explicitly.");
        Throws([] { helengine::android::AndroidAssetContentStreamSource invalid(nullptr); },"A content source requires a manager.");
        Check(manager.Handles==0,"Rejected opens must not leak handles.");
    }
    /// Native errors and premature EOF propagate while RAII still closes their failed streams.
    static void FailuresAndTruncation() {
        auto manager=Manager();
        { helengine::android::AndroidAssetStream stream(&manager,"cooked/test.bin"); manager.FailRead=true;
            Throws([&] { stream.InternalReadByte(); },"Negative SDK read must fail."); manager.FailRead=false;
            manager.FailSeek=true; Throws([&] { stream.Seek(40000,Begin); },"Native seek failure must propagate.");
            Check(stream.Position()==0,"Failed seek must not publish a new consumer position.");
        }
        Check(manager.Handles==0,"Native read/seek failures must release resources on destruction.");
        manager.FailSeek=false; manager.TruncatedBytes=5;
        Throws([&] { helengine::android::AndroidAssetStream stream(&manager,"cooked/test.bin"); std::vector<uint8_t> bytes(50000); stream.Read(bytes.data(),0,bytes.size()); },"Truncated assets must fail instead of silently returning invented data.");
        Check(manager.Handles==0,"Truncated asset failure must not leak.");
    }
    /// Empty assets, destination range overflow and seek overflow preserve stream bounds.
    static void EmptyAndBounds() {
        auto manager=Manager(0); helengine::android::AndroidAssetStream empty(&manager,"cooked/test.bin"); uint8_t byte=1;
        Check(empty.Length()==0 && empty.Read(&byte,0,1)==0 && empty.InternalReadByte()==-1 && empty.Seek(0,End)==0,"Empty assets must be legitimate empty streams.");
        Check(empty.Read(nullptr,0,0)==0,"Zero-length null reads must be harmless.");
        Throws([&] { empty.Read(nullptr,0,1); },"Nonempty null destination must fail.");
        Throws([&] { empty.Read(&byte,std::numeric_limits<size_t>::max(),2); },"Destination range overflow must fail.");
        Throws([&] { empty.Seek(std::numeric_limits<int64_t>::min(),Current); },"Negative seek overflow must fail.");
        Throws([&] { empty.Seek(std::numeric_limits<int64_t>::max(),End); },"Positive seek overflow must fail.");
        Throws([&] { empty.Seek(0,static_cast<SeekOrigin>(99)); },"Unknown seek origins must fail.");
        Throws([&] { empty.SetPosition(std::numeric_limits<size_t>::max()); },"Unsigned position overflow must fail.");
    }
};
/// Exposes test failures as a nonzero native process exit status.
int main() { try { AndroidAssetStreamTests::Run(); return 0; } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; } }
