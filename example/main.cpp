#include "pr.hpp"
#include <iostream>
#include <memory>
#include <fstream>

enum DemoMode {
    DemoMode_Point,
    DemoMode_Line,
    DemoMode_Text,
    DemoMode_Rays,
    DemoMode_Manip,
    DemoMode_Benchmark,
    DemoMode_Alembic,
    DemoMode_AlembicHoudini,
	DemoMode_AlembicCamera,
    DemoMode_Images,
};
const char* DemoModes[] = { 
    "DemoMode_Point",
    "DemoMode_Line",
    "DemoMode_Text",
    "DemoMode_Rays",
    "DemoMode_Manip",
    "DemoMode_Benchmark",
    "DemoMode_Alembic",
	"DemoMode_AlembicHoudini",
	"DemoMode_AlembicCamera",
    "DemoMode_Images",
};

class IDemo {
public:
    ~IDemo() {}
	virtual void OnUpdateCamera( pr::Camera3D* camera ) {}
    virtual void OnDraw() = 0;
    virtual void OnImGui() = 0;
    virtual pr::ITexture *GetBackground() { return nullptr; };

    pr::Camera3D camera;
};

struct PointDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        Xoshiro128StarStar random;
		for( int i = 0; i < 1024; ++i )
		{
            glm::vec3 p = GenerateUniformOnSphere(random.uniformf(), random.uniformf());
            DrawPoint(p, { 255 * color.r, 255 * color.g, 255 * color.b }, size);
        }
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Point")) {
            ImGui::SliderFloat("size", &size, 0, 32);
            ImGui::ColorPicker3("color", glm::value_ptr(color));
            ImGui::TreePop();
        }
    }
    glm::vec3 color = { 0, 1, 0 };
    float size = 5;
};

struct LineDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        Xoshiro128StarStar random;
        for (int i = 0; i < 64; ++i) {
            glm::vec3 p0 = GenerateUniformOnSphere(random.uniformf(), random.uniformf());
            glm::vec3 p1 = GenerateUniformOnSphere(random.uniformf(), random.uniformf());
            DrawLine(p0, p1, { 255 * color.r, 255 * color.g, 255 * color.b }, size);
        }
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Line")) {
            ImGui::SliderFloat("width", &size, 0, 32);
            ImGui::ColorPicker3("color", glm::value_ptr(color));
            ImGui::TreePop();
        }
    }
    glm::vec3 color = { 0, 1, 0 };
    float size = 5;
};

struct TextDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        DrawText(
            {}, text, size,
            { 255 * color.r, 255 * color.g, 255 * color.b },
            outlineWidth,
            { 255 * outlineColor.r, 255 * outlineColor.g, 255 * outlineColor.b });
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Line")) {
            ImGui::InputText("text", text, sizeof(text));
            ImGui::SliderFloat("font size", &size, 0, 256);
            ImGui::ColorPicker3("color", glm::value_ptr(color));
            ImGui::SliderFloat("outline width", &outlineWidth, 0, 32);
            ImGui::ColorPicker3("outline color", glm::value_ptr(outlineColor));
            ImGui::TreePop();
        }
        
    }
    char text[128] = "hello world";
    float size = 46;
    glm::vec3 color = { 0, 0.5f, 0 };
    float outlineWidth = 2;
    glm::vec3 outlineColor = { 1, 1, 1 };
};

/*
    x  : intersected t. -1 is no-intersected
    yzw: un-normalized normal
*/
glm::vec4 intersect_sphere(glm::vec3 ro, glm::vec3 rd, glm::vec3 o, float r) {
    float A = glm::dot(rd, rd);
    glm::vec3 S = ro - o;
    glm::vec3 SxRD = cross(S, rd);
    float D = A * r * r - glm::dot(SxRD, SxRD);

    if (D < 0.0f) {
        return glm::vec4(-1);
    }

    float B = glm::dot(S, rd);
    float sqrt_d = sqrt(D);
    float t0 = (-B - sqrt_d) / A;
    if (0.0f < t0) {
        glm::vec3 n = (rd * t0 + S);
        return glm::vec4(t0, n);
    }

    float t1 = (-B + sqrt_d) / A;
    if (0.0f < t1) {
        glm::vec3 n = (rd * t1 + S);
        return glm::vec4(t1, n);
    }
    return glm::vec4(-1);
}
glm::vec4 combine(glm::vec4 a, glm::vec4 b) {
    if (a.x < 0.0f) {
        return b;
    }
    if (b.x < 0.0f) {
        return a;
    }
    if (a.x < b.x) {
        return a;
    }
    return b;
}

struct RaysDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        if (_showWire) {
            DrawSphere({ -2, 0, 0 }, 0.5f, { 255, 255, 255 }, 16, 16);
            DrawSphere({ 0, 0, 0 }, 1.0f, { 255, 255, 255 }, 16, 16);
            DrawSphere({ 4, 0, 0 }, 2.0f, { 255, 255, 255 }, 16, 16);
        }

        Image2DRGBA8 image;
        image.allocate(GetScreenWidth() / _stride, GetScreenHeight() / _stride);

		CameraRayGenerator rayGenerator(GetCurrentViewMatrix(), GetCurrentProjMatrix(), image.width(), image.height());

        for (int j = 0; j < image.height(); ++j)
        {
            for (int i = 0; i < image.width(); ++i)
            {
				glm::vec3 ro, rd;
				rayGenerator.shoot(&ro, &rd, i, j, 0.5f, 0.5f);

                auto isect = glm::vec4(-1);
                isect = combine(isect, intersect_sphere(ro, rd, { -2, 0, 0 }, 0.5f));
                isect = combine(isect, intersect_sphere(ro, rd, { 0, 0, 0 }, 1.0f));
                isect = combine(isect, intersect_sphere(ro, rd, { 4, 0, 0 }, 2.0f));

                if (0.0f < isect.x) {
                    glm::vec3 n(isect.y, isect.z, isect.w);
                    n = glm::normalize(n);

                    glm::vec3 color = (n + glm::vec3(1.0f)) * 0.5f;
                    image(i, j) = { 255 * color.r, 255 * color.g, 255 * color.b, 255 };
                }
                else {
                    image(i, j) = { 0, 0, 0, 255 };
                }
            }
        }
        if (_texture == nullptr) {
            _texture = CreateTexture();
        }
        _texture->upload(image);
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Rays")) {
            ImGui::Checkbox("show wire", &_showWire);
            ImGui::SliderInt("stride", &_stride, 1, 8);

            if (_texture) {
                ImGui::Image(_texture, ImVec2((float)_texture->width(), (float)_texture->height()));
            }
            ImGui::TreePop();
        }
    }
    pr::ITexture *GetBackground() {
        return _texture;
    }
private:
    pr::ITexture *_texture = nullptr;
    bool _showWire = true;
    int _stride = 4;
};

struct ManipDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;
        ManipulatePosition(camera, &_position0, _size);
        ManipulatePosition(camera, &_position1, _size);
        DrawSphere(_position0, 0.5f, { 255, 128, 128 });
        DrawSphere(_position1, 0.5f, { 128, 128, 255 });
        DrawTube(_position0, _position1, 0.5f, 0.5f, { 128, 128, 128 }, 32);
    }
    void OnImGui() override {
        using namespace pr;
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Manip")) {
            ImGui::SliderFloat("size", &_size, 0, 3.0f);
            ImGui::TreePop();
        }
    }
    glm::vec3 _position0 = { -1, 0, 0 };
    glm::vec3 _position1 = { +1, 0, 0 };

    float _size = 0.3f;
};

struct BenchmarkDemo : public IDemo {
    void OnDraw() override {
        using namespace pr;

        Xoshiro128StarStar random;
        float size = 10.0f;
        for (int i = 0; i < N; ++i) {
            glm::vec3 o = { 
                glm::mix(-size, size, random.uniformf()), 
                glm::mix(-size, size, random.uniformf()), 
                glm::mix(-size, size, random.uniformf()) 
            };
            glm::vec3 d = GenerateUniformOnSphere(random.uniformf(), random.uniformf());

            glm::u8vec3 color = {
                random.uniformi() % 256,
                random.uniformi() % 256,
                random.uniformi() % 256
            };
            switch (random.uniformi() % 4) {
                case 0:
                    DrawCircle(o, d, color, 0.1f);
                    break;
                case 1:
                    DrawTube(o, o + d * 0.2f, 0.1f, 0.1f, { color });
                    break;
                case 2:
                    DrawSphere(o, 0.1f, { color });
                    break;
                case 3:
                    DrawArrow(o, o + d * 0.2f, 0.01f, { color });
            }
        }
    }
    void OnImGui() override {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
        if (ImGui::TreeNode("Benchmark")) {
            ImGui::SliderInt("N", &N, 0, 100000);
            ImGui::TreePop();
        }
    }
    float size = 5;
    int N = 32000;
};

std::vector<const char*> abcList = {
    "instancing.abc",
    "teapot.abc",
    "PointUV.abc",
    "blend_abc.abc",
    "maya_abc.abc",
    "houdini_abc.abc",
};
struct AlembicDemo : public IDemo {
    AlembicDemo()
    {
        using namespace pr;
        std::string errorMessage;
        if (_abcArchive.open(GetDataPath(abcList[0]), errorMessage) == AbcArchive::Result::Failure)
        {
            printf("Alembic Error: %s\n", errorMessage.c_str());
        }
        else
        {
            _scene = _abcArchive.readFlat(_sample_index, errorMessage);
        }

        _texture = std::shared_ptr<pr::ITexture>( pr::CreateTexture() );
        pr::Image2DRGBA8 image;
        image.load("2048x2048 Texel Density Texture 5.png");
        _texture->upload(image);
    }
    void OnDraw() override {
        using namespace pr;

        if (! _scene) {
            return;
        }
        _scene->visitPolyMesh([this](std::shared_ptr<const FPolyMeshEntity> polymesh) {
            if (polymesh->visible() == false)
            {
                return;
            }
            ColumnView<int32_t> faceCounts(polymesh->faceCounts());
            ColumnView<int32_t> indices(polymesh->faceIndices());
            ColumnView<glm::vec3> positions(polymesh->positions());
            ColumnView<glm::vec3> normals(polymesh->normals());
            ColumnView<glm::vec2> uvs(polymesh->uvs());

            pr::SetObjectTransform(polymesh->localToWorld());

            if( polymesh->isInstance() )
            {
                pr::DrawText({ 0, 0, 0 }, "instance", 14);
            }

            // Show as Textured Polygon
            if( 0 < uvs.count() )
            {
                pr::TriBegin(_texture.get());

                int indexBase = 0;
                for (int i = 0; i < faceCounts.count(); i++)
                {
                    int nVerts = faceCounts[i];
                    for (int j = 0; j < nVerts - 2 ; ++j)
                    {
                        int iv0 = indexBase;
                        int iv1 = indexBase + j + 1;
                        int iv2 = indexBase + j + 2;
                        int ip0 = indices[iv0];
                        int ip1 = indices[iv1];
                        int ip2 = indices[iv2];

                        auto revUV = [](glm::vec2 uv) { return glm::vec2 { uv.x, 1.0f - uv.y }; };
                        pr::TriVertex(positions[ip0], revUV( uvs[iv0] ), { 255, 255, 255, 255 });
                        pr::TriVertex(positions[ip1], revUV( uvs[iv1] ), { 255, 255, 255, 255 });
                        pr::TriVertex(positions[ip2], revUV( uvs[iv2] ), { 255, 255, 255, 255 });
                    }
                    indexBase += nVerts;
                }

                pr::TriEnd();
            }

            // Show as Line Geometry
            pr::PrimBegin(pr::PrimitiveMode::Lines, 2);
            for (int i = 0; i < positions.count(); i++)
            {
                glm::vec3 p = positions[i];
                pr::PrimVertex(p, { 200, 200, 200 });
            }
            int indexBase = 0;
            for (int i = 0; i < faceCounts.count(); i++)
            {
                int nVerts = faceCounts[i];
                for (int j = 0; j < nVerts; ++j)
                {
                    int i0 = indices[indexBase + j];
                    int i1 = indices[indexBase + (j + 1) % nVerts];
                    pr::PrimIndex(i0);
                    pr::PrimIndex(i1);
                }
                indexBase += nVerts;
            }
            pr::PrimEnd();

            // Normal
            if (normals.empty() == false)
            {
                pr::PrimBegin(pr::PrimitiveMode::Lines);
                int indexBase = 0;
                for (int i = 0; i < faceCounts.count(); i++)
                {
                    int nVerts = faceCounts[i];

                    // adaptive n length
                    OnlineMean<float> d2;
                    for (int j = 0; j < nVerts; ++j)
                    {
                        int i0 = indices[indexBase + j];
                        int i1 = indices[indexBase + (j + 1) % nVerts];
                        glm::vec3 d = positions[i0] - positions[i1];
                        d2.addSample(glm::dot(d, d));
                    }
                    float edge = std::sqrt(d2.mean());
                    for (int j = 0; j < nVerts; ++j)
                    {
                        glm::vec3 p = positions[indices[indexBase + j]];
                        glm::vec3 n = normals[indexBase + j];
                        pr::PrimVertex(p, { 255, 128, 128 });
                        pr::PrimVertex(p + n * edge * 0.4f, { 255, 128, 128 });
                    }
                    indexBase += nVerts;
                }
                pr::PrimEnd();
            }

            pr::SetObjectIdentify();
        });
    }
    void OnImGui() override {
        if (ImGui::Combo("File", &_abcFileIndex, abcList.data(), abcList.size()))
        {
            using namespace pr;
            std::string errorMessage;
            if (_abcArchive.open(GetDataPath(abcList[_abcFileIndex]), errorMessage) == AbcArchive::Result::Failure)
            {
                printf("Alembic Error: %s\n", errorMessage.c_str());
            }
            else
            {
                _scene = _abcArchive.readFlat(_sample_index, errorMessage);
            }
        }
        if (ImGui::SliderInt("sample", &_sample_index, 0, _abcArchive.frameCount()))
        {
            std::string errorMessage;
            _scene = _abcArchive.readFlat(_sample_index, errorMessage);
        }
    }
    int _abcFileIndex = 0;
    pr::AbcArchive _abcArchive;
    int _sample_index = 0;
    std::shared_ptr<pr::FScene> _scene;
    std::shared_ptr<pr::ITexture> _texture;
};

struct AlembicHoudiniDemo : public IDemo {
    AlembicHoudiniDemo()
    {
        using namespace pr;
        pr::AbcArchive abcArchive;
        std::string errorMessage;
        if (abcArchive.open(GetDataPath("edobee.abc"), errorMessage) == AbcArchive::Result::Failure)
        {
            printf("Alembic Error: %s\n", errorMessage.c_str());
        }
        else
        {
            _scene = abcArchive.readFlat(0, errorMessage);
        }
    }
    void OnDraw() override {
        using namespace pr;

        if (!_scene) {
            return;
        }
        _scene->visitPolyMesh([](std::shared_ptr<const FPolyMeshEntity> polymesh) {
            if (polymesh->visible() == false)
            {
                return;
            }
            ColumnView<int32_t> faceCounts(polymesh->faceCounts());
            ColumnView<int32_t> indices(polymesh->faceIndices());
            ColumnView<glm::vec3> positions(polymesh->positions());
            ColumnView<glm::vec3> normals(polymesh->normals());

            pr::SetObjectTransform(polymesh->localToWorld());

            auto sheet = polymesh->attributeSpreadsheet(pr::AttributeSpreadsheetType::Points);
            auto Cd = sheet->columnAsVector3("Cd");

            // Geometry
            pr::PrimBegin(pr::PrimitiveMode::Lines);
            for (int i = 0; i < positions.count(); i++)
            {
                glm::vec3 p = positions[i];
                glm::ivec3 color = glm::ivec3(Cd->get(i) * 255.0f + 0.5f);
                pr::PrimVertex(p, { color });
            }
            int indexBase = 0;
            for (int i = 0; i < faceCounts.count(); i++)
            {
                int nVerts = faceCounts[i];
                for (int j = 0; j < nVerts; ++j)
                {
                    int i0 = indices[indexBase + j];
                    int i1 = indices[indexBase + (j + 1) % nVerts];
                    pr::PrimIndex(i0);
                    pr::PrimIndex(i1);
                }
                indexBase += nVerts;
            }
            pr::PrimEnd();
            pr::SetObjectIdentify();
        });
    }
    void OnImGui() override {

    }
    std::shared_ptr<pr::FScene> _scene;
};

struct AlembicCameraDemo : public IDemo
{
	AlembicCameraDemo()
	{
		using namespace pr;
		_archive = std::shared_ptr<pr::AbcArchive>( new pr::AbcArchive() );
		std::string errorMessage;
		if( _archive->open( GetDataPath( "camera.abc" ), errorMessage ) == AbcArchive::Result::Failure )
		{
			printf( "Alembic Error: %s\n", errorMessage.c_str() );
		}
		_scene = _archive->readFlat( _frame, errorMessage );
	}
	virtual void OnUpdateCamera( pr::Camera3D* camera ) 
    {
		_scene->visitCamera( [&]( std::shared_ptr<const pr::FCameraEntity> cameraEntity )
		{ 
            *camera = cameraFromEntity( cameraEntity.get() );
		} );
    }
	void OnDraw() override
	{
		using namespace pr;
		std::string errorMessage;
        _scene = _archive->readFlat( _frame, errorMessage );

		if( !_scene )
		{
			return;
		}
		_scene->visitCamera( []( std::shared_ptr<const FCameraEntity> camera )
	    {
            pr::SetObjectTransform( camera->localToWorld() );
            DrawXYZAxis();
			pr::SetObjectIdentify();
		} );
		_scene->visitPolyMesh( []( std::shared_ptr<const FPolyMeshEntity> polymesh )
		{
			if( polymesh->visible() == false )
			{
				return;
			}
			ColumnView<int32_t> faceCounts( polymesh->faceCounts() );
			ColumnView<int32_t> indices( polymesh->faceIndices() );
			ColumnView<glm::vec3> positions( polymesh->positions() );
			ColumnView<glm::vec3> normals( polymesh->normals() );

			pr::SetObjectTransform( polymesh->localToWorld() );

			auto sheet = polymesh->attributeSpreadsheet( pr::AttributeSpreadsheetType::Points );

			// Geometry
			pr::PrimBegin( pr::PrimitiveMode::Lines );
			for( int i = 0; i < positions.count(); i++ )
			{
				glm::vec3 p = positions[i];
				pr::PrimVertex( p, { 255, 255, 255 } );
			}
			int indexBase = 0;
			for( int i = 0; i < faceCounts.count(); i++ )
			{
				int nVerts = faceCounts[i];
				for( int j = 0; j < nVerts; ++j )
				{
					int i0 = indices[indexBase + j];
					int i1 = indices[indexBase + ( j + 1 ) % nVerts];
					pr::PrimIndex( i0 );
					pr::PrimIndex( i1 );
				}
				indexBase += nVerts;
			}
			pr::PrimEnd();
			pr::SetObjectIdentify();
		} );
	}
	void OnImGui() override
	{
		ImGui::SetNextItemOpen( true, ImGuiCond_Once );
		if( ImGui::TreeNode( "Abc" ) )
		{
			ImGui::SliderInt( "frame", &_frame, 0, _archive->frameCount() - 1 );
			ImGui::TreePop();
		}
	}

	std::shared_ptr<pr::AbcArchive> _archive;
	std::shared_ptr<pr::FScene> _scene;
	int _frame = 0;
};


struct ImagesDemo : public IDemo {
    ImagesDemo()
    {
		std::shared_ptr<pr::ITexture> image0 = std::shared_ptr<pr::ITexture>( pr::CreateTexture() );
		{
			pr::Image2DRGBA8 image;
			PR_ASSERT( image.load( "edobee.jpg" ) == pr::Result::Sucess );
			image0->upload( image );
		}
		_images.push_back( image0 );

		std::shared_ptr<pr::ITexture> image1 = std::shared_ptr<pr::ITexture>( pr::CreateTexture() );
		{
			pr::Image2DRGBA32 image;
			PR_ASSERT( image.loadFromHDR( "blaubeuren_night_1k.hdr" ) == pr::Result::Sucess );
			image1->upload( image.map( []( glm::vec4 c ) {
				return glm::pow( c, glm::vec4( 0.454545f ) );
			} ) );
		}
		_images.push_back( image1 );

		std::shared_ptr<pr::ITexture> image2 = std::shared_ptr<pr::ITexture>( pr::CreateTexture() );
		{
			pr::Image2DRGBA32 image;
			PR_ASSERT( image.loadFromEXR( "StillLife.exr" ) == pr::Result::Sucess );
			image2->upload( image.map( []( glm::vec4 c ) {
				return glm::pow( c, glm::vec4( 0.454545f ) );
			} ) );
		}
		_images.push_back( image2 );

        std::vector<std::string> layers;
        pr::LayerListFromEXR(layers, "multilayer0001.exr");
        for (int i = 0; i < layers.size(); ++i)
        {
            pr::Image2DRGBA32 image;
            image.loadFromEXR("multilayer0001.exr", layers[i].c_str());
            Layer layer = {
                layers[i],
                std::shared_ptr<pr::ITexture>( pr::CreateTexture() )
            };
            layer.image->upload(image);
            _layers.emplace_back(std::move(layer));
        }
    }
    void OnDraw() override {

    }
    void OnImGui() override {
        using namespace pr;

        if (ImGui::Button("Save Images -> saveX.png"))
        {
            {
                Image2DRGBA8 image;
                PR_ASSERT(image.load("edobee.jpg") == pr::Result::Sucess);
                image.saveAsPng("saveA.png");
            }
            {
                Image2DRGBA32 image;
                PR_ASSERT(image.loadFromHDR("blaubeuren_night_1k.hdr") == pr::Result::Sucess);
                image.saveAsHDR("saveB.hdr");
            }
            {
                Image2DRGBA32 image;
                PR_ASSERT(image.loadFromEXR("StillLife.exr") == pr::Result::Sucess);
                image.saveAsEXR("saveC.exr");
            }

            {
				std::vector<std::string> layers;
				LayerListFromEXR( layers, "multilayer0001.exr" );
                std::vector<std::shared_ptr<Image2DRGBA32>> images;

                MultiLayerExrWriter writer;
				for( int i = 0; i < layers.size(); ++i )
				{
                    std::shared_ptr<Image2DRGBA32> image = std::make_shared<Image2DRGBA32>();
                    images.push_back(image);
					image->loadFromEXR( "multilayer0001.exr", layers[i].c_str());

                    writer.add(image.get(), layers[i]);
				}
                Result r = writer.saveAs("saveD.exr");
                PR_ASSERT(r == Result::Sucess);
            }
        }

        if (ImGui::BeginTabBar("Images", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Standard Images"))
            {
				for( auto image : _images )
				{
					ImGui::Image( image.get(), ImVec2( image->width(), image->height() ) );
				}
				ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("MultiLayer Exr"))
            {
                for (int i = 0; i < _layers.size(); ++i)
                {
                    ImGui::Text(_layers[i].name.c_str());
                    ImGui::Image(_layers[i].image.get(), ImVec2(_layers[i].image->width(), _layers[i].image->height()));
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

    }

    std::vector< std::shared_ptr<pr::ITexture> > _images;

    struct Layer {
        std::string name;
        std::shared_ptr<pr::ITexture> image;
    };
    std::vector<Layer> _layers;
};

std::vector<IDemo*> demos;

int main() {
    using namespace pr;

    SetDataDir(JoinPath(ExecutableDir(), "../data"));

    SetEnableMultiThreadExrProceccing( true );
    //ParallelFor(10, [](int i) {
    //    ChromeTraceTimer timer( ChromeTraceTimer::AddMode::Auto );
    //    timer.label("p[%d]", i);
    //    timer["value"] = std::to_string(i);

    //    SleepForMilliSeconds( rand() % 100 );
    //    {
    //        int n = rand() % 5;
    //        for (int i = 0; i < n; ++i)
    //        {
    //            ChromeTraceTimer timer( ChromeTraceTimer::AddMode::Auto );
    //            SleepForMilliSeconds(10);
    //        }
    //    }
    //});
    //std::ofstream ofs( GetDataPath("chrome.json") );
    //ofs << ChromeTraceGetTrace();
    //ofs.close();

    SetFileDropCallback([](std::vector<std::string> files) {
        for (auto file : files)
        {
            std::cout << file << std::endl;
            std::cout << GetPathBasename(file) << std::endl;
            std::cout << GetPathBasenameWithoutExtension(file) << std::endl;
            std::cout << GetPathDirname(file) << std::endl;
            std::cout << GetPathExtension(file) << std::endl;
            std::cout << ChangePathExtension(file, "hogehogehoge") << std::endl;
        }
    });

    Config config;
    config.ScreenWidth = 1920;
    config.ScreenHeight = 1080;
    config.SwapInterval = 1;
    Initialize(config);

    demos = {
        new PointDemo(),
        new LineDemo(),
        new TextDemo(),
        new RaysDemo(),
        new ManipDemo(),
        new BenchmarkDemo(),
        new AlembicDemo(),
        new AlembicHoudiniDemo(),
        new AlembicCameraDemo(),
        new ImagesDemo(),
    };

    pr::SetDepthTest(true);

    Camera3D camera;
    camera.origin = { 4, 4, 4 };
    camera.lookat = { 0, 0, 0 };
    camera.zNear = 0.1f;
    camera.zUp = false;

    double e = GetElapsedTime();

    bool showImGuiDemo = false;
    float fontSize = 16.0f;
    int demoMode = DemoMode_AlembicCamera;

    while (pr::NextFrame() == false) {
        if (IsImGuiUsingMouse() == false) {
            UpdateCameraBlenderLike(&camera);
        }
		demos[demoMode]->OnUpdateCamera( &camera );
        demos[demoMode]->camera = camera;

        if (ITexture *bg = demos[demoMode]->GetBackground()) {
            ClearBackground(bg);
        }
        else {
            ClearBackground(0.1f, 0.1f, 0.1f, 1);
        }

        BeginCamera(camera);
        PushGraphicState();

        DrawGrid(GridAxis::XZ, 1.0f, 10, { 128, 128, 128 });
        DrawXYZAxis(1.0f);

        demos[demoMode]->OnDraw();
        
        PopGraphicState();
        EndCamera();

        // DrawTextScreen(20, 20, R"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~)");

        BeginImGui();

        ImGui::SetNextWindowSize({ 500, 800 }, ImGuiCond_Once);
        ImGui::Begin("Panel");
        ImGui::Text("fps = %f", GetFrameRate());
        ImGui::Combo("Demo Mode", &demoMode, DemoModes, IM_ARRAYSIZE(DemoModes));
        ImGui::Checkbox("show ImGui Demo", &showImGuiDemo);
        
        demos[demoMode]->OnImGui();

        ImGui::End();

        if (showImGuiDemo) {
            ImGui::ShowDemoWindow();
        }
        EndImGui();
    }

    demos.clear();

    pr::CleanUp();
}
