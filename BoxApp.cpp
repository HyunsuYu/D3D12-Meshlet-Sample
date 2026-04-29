//***************************************************************************************
// BoxApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Shows how to draw a box in Direct3D 12.
//
// Controls:
//   Hold the left mouse button down and move the mouse to rotate.
//   Hold the right mouse button down and move the mouse to zoom in and out.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

#include <fbxsdk.h>

#include <meshoptimizer.h>
#include "geometrycentral/surface/geodesic_centroidal_voronoi_tessellation.h"

#include "geometrycentral/surface/manifold_surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"
#include "geometrycentral/surface/meshio.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

extern "C" { __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001; }
extern "C" { __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; }

size_t meshletCount;

struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
    float pad;
};

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

struct Meshlet {
    UINT32 VertCount;
    UINT32 VertOffset;
    UINT32 PrimCount;
    UINT32 PrimOffset;
};

class BoxApp : public D3DApp
{
public:
	BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp& rhs) = delete;
    BoxApp& operator=(const BoxApp& rhs) = delete;
	~BoxApp();

	virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void BuildDescriptorHeaps();
	void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildBoxGeometry();
    void BuildPSO();

private:
    
    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

    std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;

    ComPtr<ID3DBlob> mvsByteCode = nullptr;
    ComPtr<ID3DBlob> mpsByteCode = nullptr;
    ComPtr<ID3DBlob> mmsByteCode = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    ComPtr<ID3D12PipelineState> mPSO = nullptr;

    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = XM_PIDIV4;
    float mRadius = 5.0f;

    POINT mLastMousePos;
};

int numTabs = 0;
void PrintTabs()
{
    for (int i = 0; i < numTabs; i++)
        printf("\t");
}

FbxString GetAttributeTypeName(FbxNodeAttribute::EType type)
{
    switch (type)
    {
    case FbxNodeAttribute::eUnknown: return "unidentified";
    case FbxNodeAttribute::eNull: return "null";
    case FbxNodeAttribute::eMarker: return "marker";
    case FbxNodeAttribute::eSkeleton: return "skeleton";
    case FbxNodeAttribute::eMesh: return "mesh";
    case FbxNodeAttribute::eNurbs: return "nurbs";
    case FbxNodeAttribute::ePatch: return "patch";
    case FbxNodeAttribute::eCamera: return "camera";
    case FbxNodeAttribute::eCameraStereo: return "stereo";
    case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
    case FbxNodeAttribute::eLight: return "light";
    case FbxNodeAttribute::eOpticalReference: return "optical reference";
    case FbxNodeAttribute::eOpticalMarker: return "marker";
    case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
    case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
    case FbxNodeAttribute::eBoundary: return "boundary";
    case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
    case FbxNodeAttribute::eShape: return "shape";
    case FbxNodeAttribute::eLODGroup: return "lodgroup";
    case FbxNodeAttribute::eSubDiv: return "subdiv";
    default: return "unknown";
    }
}

void PrintAttribute(FbxNodeAttribute* pAttribute, std::vector<::Vertex>& vertexs, std::vector<std::uint16_t>& indics)
{
    if (!pAttribute) return;

    FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
    FbxString attrName = pAttribute->GetName();
    PrintTabs();

    if (pAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    {
        FbxMesh* mesh = (FbxMesh*)pAttribute;
        fbxsdk::FbxVector4* points = mesh->GetControlPoints();
        for (int index = 0; index < mesh->GetControlPointsCount(); index++)
        {
			::Vertex v;
            v.Pos = XMFLOAT3(static_cast<float>(points[index][0]),
                static_cast<float>(points[index][1]),
				static_cast<float>(points[index][2]));
			v.Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			vertexs.push_back(v);
        }

        int* polygonVertices = mesh->GetPolygonVertices();
        for (int index = 0; index < mesh->GetPolygonVertexCount(); index++)
        {
            PrintTabs();
            printf("\t<polygonVertex index='%d' controlPointIndex='%d'/>\n",
                index,
                polygonVertices[index]
            );
			indics.push_back(polygonVertices[index]);
        }
    }
}

void PrintNode(FbxNode* pNode, std::vector<::Vertex>& vertexs, std::vector<std::uint16_t>& indics)
{
    PrintTabs();
    const char* nodeName = pNode->GetName();
    FbxDouble3 translation = pNode->LclTranslation.Get();
    FbxDouble3 rotation = pNode->LclRotation.Get();
    FbxDouble3 scaling = pNode->LclScaling.Get();

    printf("<node name='%s' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' scaling='(%f, %f, %f)'>\n",
        nodeName,
        translation[0], translation[1], translation[2],
        rotation[0], rotation[1], rotation[2],
        scaling[0], scaling[1], scaling[2]
    );
    numTabs++;

    for (int i = 0; i < pNode->GetNodeAttributeCount(); i++)
    {
        PrintAttribute(pNode->GetNodeAttributeByIndex(i), vertexs, indics);
    }

    for (int j = 0; j < pNode->GetChildCount(); j++)
    {
        PrintNode(pNode->GetChild(j), vertexs, indics);
    }

    numTabs--;
    PrintTabs();
    printf("</node>\n");
}

XMFLOAT4 GetRandomColor(size_t index)
{
    unsigned int hash = (unsigned int)index;
    hash = hash * 0x9E3779B9u;
    hash ^= (hash >> 16); hash *= 0x85EBCA6Bu;
    hash ^= (hash >> 13); hash *= 0xC2B2AE35u;
    hash ^= (hash >> 16);
    return XMFLOAT4((hash & 0xFF) / 255.0f, ((hash >> 8) & 0xFF) / 255.0f, ((hash >> 16) & 0xFF) / 255.0f, 1.0f);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
    




    //const char* lFilename = "criffp_b.fbx";
    //FbxManager* lSdkManager = FbxManager::Create();

    //FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
    //lSdkManager->SetIOSettings(ios);

    //FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");

    //if (!lImporter->Initialize(lFilename, -1, lSdkManager->GetIOSettings())) {
    //    MessageBox(0, L"Call to FbxImporter::Initialize() failed.", 0, 0);
    //    //printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
    //    exit(-1);
    //}

    //// Create a new scene so that it can be populated by the imported file.
    //FbxScene* lScene = FbxScene::Create(lSdkManager, "myScene");

    //// Import the contents of the file into the scene.
    //lImporter->Import(lScene);

    //// The file is imported, so get rid of the importer.
    //lImporter->Destroy();

    //FbxNode* lRootNode = lScene->GetRootNode();
    //if (lRootNode) {
    //    for (int i = 0; i < lRootNode->GetChildCount(); i++)
    //        PrintNode(lRootNode->GetChild(i));
    //}
    //// Destroy the SDK manager and all the other objects it was handling.
    //lSdkManager->Destroy();




    try
    {
        BoxApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

BoxApp::BoxApp(HINSTANCE hInstance)
: D3DApp(hInstance) 
{
}

BoxApp::~BoxApp()
{
}

bool BoxApp::Initialize()
{
    if(!D3DApp::Initialize())
		return false;
	
    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
 
    BuildDescriptorHeaps();
	BuildConstantBuffers();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildBoxGeometry();
    BuildPSO();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();



    D3D12_FEATURE_DATA_D3D12_OPTIONS7 featureData = {};
    md3dDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &featureData, sizeof(featureData));

    if (featureData.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED)
    {
        MessageBox(0, L"이 그래픽 카드는 Mesh Shader를 지원하지 않습니다.", 0, 0);
        return false;
    }

	return true;
}

void BoxApp::OnResize()
{
	D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void BoxApp::Update(const GameTimer& gt)
{
    // Convert Spherical to Cartesian coordinates.
    float x = mRadius*sinf(mPhi)*cosf(mTheta);
    float z = mRadius*sinf(mPhi)*sinf(mTheta);
    float y = mRadius*cosf(mPhi);

    // Build the view matrix.
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&mView, view);

    XMMATRIX world = XMLoadFloat4x4(&mWorld);
    XMMATRIX proj = XMLoadFloat4x4(&mProj);
    XMMATRIX worldViewProj = world*view*proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	ObjectConstants objConstants;
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    mObjectCB->CopyData(0, objConstants);
}

void BoxApp::Draw(const GameTimer& gt)
{
    // 1. 커맨드 할당자 및 커맨드 리스트 재설정
    // GPU가 이전 프레임의 명령을 모두 수행했는지 확인(FlushCommandQueue)한 후여야 합니다.
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

    // 2. 뷰포트 및 시저 사각형 설정
    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // 3. 리소스 배리어: Present -> RenderTarget
    // 화면에 출력 중이던 버퍼를 그리기 가능한 상태로 변경
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // 4. 렌더 타겟 및 깊이 버퍼 클리어
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // 5. 렌더 타겟 지정
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // (1) 서술자 힙(Descriptor Heap) 설정
    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    // (2) 루트 시그니처 설정
    mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

    // (3) 루트 디스크립터 테이블 설정
    // 0번 슬롯(t0~t3)에 SRV 힙의 시작 핸들을 전달
    mCommandList->SetGraphicsRootConstantBufferView(0, mObjectCB->Resource()->GetGPUVirtualAddress());
    mCommandList->SetGraphicsRootDescriptorTable(1, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    // (4) 메쉬 쉐이더 실행 (Dispatch)
    mCommandList->DispatchMesh(meshletCount, 1, 1);

    // 6. 리소스 배리어: RenderTarget -> Present
    mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // 7. 명령 기록 종료
    ThrowIfFailed(mCommandList->Close());

    // 8. 커맨드 큐에 실행 요청
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // 9. 화면 교체 (Present)
    ThrowIfFailed(mSwapChain->Present(0, 0));
    mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // 10. 동기화 (GPU가 다 그릴 때까지 대기)
    FlushCommandQueue();
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.5f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.5f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 1.0f, 1200.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}

void BoxApp::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
        IID_PPV_ARGS(&mCbvHeap)));
}

void BoxApp::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();
    // Offset to the ith object constant buffer in the buffer.
    int boxCBufIndex = 0;
	cbAddress += boxCBufIndex*objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	md3dDevice->CreateConstantBufferView(
		&cbvDesc,
		mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void BoxApp::BuildRootSignature()
{
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

    slotRootParameter[0].InitAsConstantBufferView(0);

	// Create a single descriptor table of CBVs.
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}

void BoxApp::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;
    
	//mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	//mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

    mpsByteCode = d3dUtil::CompileShaderDXC(L"Shaders\\ms_meshlet_debug.hlsl", L"ps_main", L"ps_6_5");
    mmsByteCode = d3dUtil::CompileShaderDXC(L"Shaders\\ms_meshlet_debug.hlsl", L"ms_main", L"ms_6_7");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void BoxApp::BuildBoxGeometry()
{
    const char* lFilename = "stanford-bunny.fbx";
    FbxManager* lSdkManager = FbxManager::Create();

    FbxIOSettings* ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
    lSdkManager->SetIOSettings(ios);

    FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");

    if (!lImporter->Initialize(lFilename, -1, lSdkManager->GetIOSettings())) {
        MessageBox(0, L"Call to FbxImporter::Initialize() failed.", 0, 0);
        exit(-1);
    }

    // Create a new scene so that it can be populated by the imported file.
    FbxScene* lScene = FbxScene::Create(lSdkManager, "myScene");

    // Import the contents of the file into the scene.
    lImporter->Import(lScene);

    // The file is imported, so get rid of the importer.
    lImporter->Destroy();

    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    FbxNode* lRootNode = lScene->GetRootNode();
    if (lRootNode)
    {
        for (int i = 0; i < lRootNode->GetChildCount(); i++)
        {
            PrintNode(lRootNode->GetChild(i), vertices, indices);
        }
    }

    lSdkManager->Destroy();


#pragma region meshoptimizer
    constexpr size_t MaxVertices = 64;
    constexpr size_t MaxTriangles = 124;

    size_t maxMashlets = meshopt_buildMeshletsBound(indices.size(), MaxVertices, MaxTriangles);

    std::vector<meshopt_Meshlet> meshlets(maxMashlets);
    std::vector<UINT32> polygonVertices(maxMashlets * MaxVertices);
    std::vector<UINT8> polygonTriangles(maxMashlets * MaxTriangles * 3);

    constexpr float ConeWeight = 0.f;

    meshletCount = meshopt_buildMeshlets(meshlets.data()
        , polygonVertices.data()
        , polygonTriangles.data()
        , indices.data()
        , indices.size()
        , &vertices[0].Pos.x
        , vertices.size()
        , sizeof(Vertex)
        , MaxVertices
        , MaxTriangles
        , ConeWeight);

    std::vector<Meshlet> gpuMeshlets;
    gpuMeshlets.reserve(meshletCount);

    for (size_t i = 0; i < meshletCount; ++i) {
        Meshlet m;
        m.VertCount = meshlets[i].vertex_count;
        m.VertOffset = meshlets[i].vertex_offset;
        m.PrimCount = meshlets[i].triangle_count;
        m.PrimOffset = meshlets[i].triangle_offset;
        gpuMeshlets.push_back(m);
    }

    while (polygonTriangles.size() % 4 != 0) {
        polygonTriangles.push_back(0); // 4의 배수가 될 때까지 0으로 채움
    }
#pragma endregion

#pragma region GCVT
    //std::vector<std::vector<size_t>> polygons;
    //size_t triangleCount = indices.size() / 3;
    //polygons.reserve(triangleCount);

    //for (size_t i = 0; i < triangleCount; i++)
    //{
    //    // 3개씩 끊어서 폴리곤 하나를 만듭니다.
    //    // 주의: indices는 uint16_t 또는 uint32_t이지만, geometry-central은 size_t를 씁니다.
    //    std::vector<size_t> face;
    //    face.push_back((size_t)indices[i * 3 + 0]);
    //    face.push_back((size_t)indices[i * 3 + 1]);
    //    face.push_back((size_t)indices[i * 3 + 2]);
    //    polygons.push_back(face);
    //}

    //// (2) ManifoldSurfaceMesh 생성
    //std::unique_ptr<geometrycentral::surface::ManifoldSurfaceMesh> mesh;
    //std::unique_ptr<geometrycentral::surface::VertexPositionGeometry> geometry;

    //// 생성자를 통해 토폴로지 구성
    //mesh = std::make_unique<geometrycentral::surface::ManifoldSurfaceMesh>(polygons);

    //// (3) 정점 위치 데이터 주입 (Geometry 구성)
    //geometry = std::make_unique<geometrycentral::surface::VertexPositionGeometry>(*mesh);

    //// geometry-central은 내부적으로 정점 인덱싱을 다시 할 수 있으므로, 
    //// mesh->getVertex(i)를 통해 순서대로 값을 넣어줍니다.
    //for (size_t i = 0; i < vertices.size(); i++)
    //{
    //    // vertices[i].Pos는 XMFLOAT3이므로 geometry-central의 Vector3로 변환
    //    geometrycentral::Vector3 pos;
    //    pos.x = vertices[i].Pos.x;
    //    pos.y = vertices[i].Pos.y;
    //    pos.z = vertices[i].Pos.z;

    //    // i번 정점의 위치 설정
    //    geometry->inputVertexPositions[mesh->vertex(i)] = pos;
    //}

    //// ---------------------------------------------------------
    //// 2. GCVT 실행
    //// ---------------------------------------------------------

    //// (1) 옵션 설정 (필요에 따라 값 조정)
    //geometrycentral::surface::VoronoiOptions gcvtOptions = geometrycentral::surface::defaultVoronoiOptions;
    //gcvtOptions.nSites = meshletCount;
    //gcvtOptions.iterations = 1;

    //// (2) GCVT 함수 호출
    //// computeGeodesicCentroidalVoronoiTessellation(ManifoldSurfaceMesh& mesh, IntrinsicGeometryInterface& geom, VoronoiOptions options);
    //// *mesh, *geometry 형태로 참조를 넘겨줍니다.
    //geometrycentral::surface::VoronoiResult gcvtResult = computeGeodesicCentroidalVoronoiTessellation(*mesh, *geometry, gcvtOptions);


    ////
    //std::vector<size_t> vertexToSiteID(mesh->nVertices());

    //for (size_t i = 0; i < mesh->nVertices(); i++) {
    //    geometrycentral::surface::SurfacePoint vPoint = mesh->vertex(i); // Vertex 타입을 SurfacePoint로 취급 가능하거나 내부 인덱스 사용

    //    double maxVal = -std::numeric_limits<double>::infinity();
    //    size_t bestSite = 0;

    //    // 모든 Site를 순회하며 누가 가장 영향력이 큰지 확인
    //    for (size_t s = 0; s < gcvtResult.siteDistributions.size(); s++) {
    //        // geometry-central의 VertexData는 mesh->vertex(i)로 접근
    //        double val = gcvtResult.siteDistributions[s][mesh->vertex(i)];

    //        if (val > maxVal) {
    //            maxVal = val;
    //            bestSite = s;
    //        }
    //    }

    //    vertexToSiteID[i] = bestSite;

    //    // 시각화: 정점 색상 변경
    //    vertices[i].Color = GetRandomColor(bestSite);
    //}

    //// (2) Face Partitioning (Majority Vote)
    //// 각 Site(Meshlet Cluster)별로 포함될 삼각형 인덱스를 모읍니다.
    //std::vector<std::vector<uint32_t>> clusterIndices(result.siteDistributions.size());

    //for (size_t i = 0; i < nFaces; i++) {
    //    uint32_t i0 = indices[i * 3 + 0];
    //    uint32_t i1 = indices[i * 3 + 1];
    //    uint32_t i2 = indices[i * 3 + 2];

    //    size_t s0 = vertexToSiteID[i0];
    //    size_t s1 = vertexToSiteID[i1];
    //    size_t s2 = vertexToSiteID[i2];

    //    // 다수결 원칙: 2개 이상 같은 Site가 나오면 거기로 배정
    //    size_t ownerSite = s0;
    //    if (s1 == s2) ownerSite = s1;
    //    else if (s0 == s2) ownerSite = s2;
    //    else if (s0 == s1) ownerSite = s0;

    //    clusterIndices[ownerSite].push_back(i0);
    //    clusterIndices[ownerSite].push_back(i1);
    //    clusterIndices[ownerSite].push_back(i2);
    //}

    //// ========================================================
    //// 4. Meshlet Generation (Post-Processing w/ Meshoptimizer)
    //// ========================================================

    //std::vector<Meshlet> finalMeshlets;
    //std::vector<uint32_t> finalPolyVerts;
    //std::vector<uint8_t> finalPolyTris;

    //constexpr size_t MaxVertices = 64;
    //constexpr size_t MaxTriangles = 124;
    //constexpr float ConeWeight = 0.0f;

    //// 각 GCVT 클러스터 내부에서 하드웨어 제한에 맞게 다시 쪼개기
    //for (size_t siteID = 0; siteID < clusterIndices.size(); siteID++) {
    //    std::vector<uint32_t>& localIndices = clusterIndices[siteID];
    //    if (localIndices.empty()) continue;

    //    size_t maxMeshletsLocal = meshopt_buildMeshletsBound(localIndices.size(), MaxVertices, MaxTriangles);
    //    std::vector<meshopt_Meshlet> localMeshlets(maxMeshletsLocal);
    //    std::vector<uint32_t> localPolyVerts(maxMeshletsLocal * MaxVertices);
    //    std::vector<uint8_t> localPolyTris(maxMeshletsLocal * MaxTriangles * 3);

    //    size_t count = meshopt_buildMeshlets(
    //        localMeshlets.data(), localPolyVerts.data(), localPolyTris.data(),
    //        localIndices.data(), localIndices.size(),
    //        &vertices[0].Pos.x, vertices.size(), sizeof(Vertex),
    //        MaxVertices, MaxTriangles, ConeWeight
    //    );

    //    for (size_t k = 0; k < count; k++) {
    //        meshopt_Meshlet& m = localMeshlets[k];
    //        Meshlet gpuMeshlet;

    //        gpuMeshlet.VertCount = m.vertex_count;
    //        gpuMeshlet.PrimCount = m.triangle_count;
    //        gpuMeshlet.VertOffset = (uint32_t)finalPolyVerts.size();
    //        gpuMeshlet.PrimOffset = (uint32_t)finalPolyTris.size();

    //        for (size_t v = 0; v < m.vertex_count; v++) {
    //            finalPolyVerts.push_back(localPolyVerts[m.vertex_offset + v]);
    //        }

    //        for (size_t t = 0; t < m.triangle_count * 3; t++) {
    //            finalPolyTris.push_back(localPolyTris[m.triangle_offset + t]);
    //        }

    //        while (finalPolyTris.size() % 4 != 0) {
    //            finalPolyTris.push_back(0);
    //        }

    //        finalMeshlets.push_back(gpuMeshlet);
    //    }
    //}

    //meshletCount = finalMeshlets.size();
#pragma endregion

    // 1. Vertex Buffer (StructuredBuffer<Vertex>) 생성
    mVertexBufferGPU = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(),
        mCommandList.Get(),
        vertices.data(),
        vertices.size() * sizeof(Vertex),
        mVertexUploadBuffer);

    // 2. Meshlet Buffer (StructuredBuffer<Meshlet>) 생성
    mMeshletBufferGPU = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(),
        mCommandList.Get(),
        gpuMeshlets.data(),
        gpuMeshlets.size() * sizeof(Meshlet),
        mMeshletUploadBuffer);

    // 3. Polygon Vertices (StructuredBuffer<uint>) 생성
    mMeshletVerticesGPU = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(),
        mCommandList.Get(),
        polygonVertices.data(),
        polygonVertices.size() * sizeof(UINT32),
        mMeshletVerticesUploadBuffer);

    // 4. Polygon Primitives (ByteAddressBuffer) 생성
    // uint8_t 데이터이므로 StructuredBuffer로는 X(최소 4바이트 스트라이드 필요)
    mMeshletPrimsGPU = d3dUtil::CreateDefaultBuffer(
        md3dDevice.Get(),
        mCommandList.Get(),
        polygonTriangles.data(),
        polygonTriangles.size() * sizeof(UINT8),
        mMeshletPrimsUploadBuffer);

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN; // StructuredBuffer는 포맷을 모름
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = (UINT)vertices.size();
    srvDesc.Buffer.StructureByteStride = sizeof(Vertex); // 구조체 크기
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    md3dDevice->CreateShaderResourceView(mVertexBufferGPU.Get(), &srvDesc, hDescriptor);


    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

    // Meshlets SRV (StructuredBuffer)
    srvDesc.Buffer.NumElements = (UINT)gpuMeshlets.size();
    srvDesc.Buffer.StructureByteStride = sizeof(Meshlet);

    md3dDevice->CreateShaderResourceView(mMeshletBufferGPU.Get(), &srvDesc, hDescriptor);


    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

    // Polygon Vertices SRV (StructuredBuffer<uint>)
    srvDesc.Buffer.NumElements = (UINT)polygonVertices.size();
    srvDesc.Buffer.StructureByteStride = sizeof(UINT32); // 4바이트

    md3dDevice->CreateShaderResourceView(mMeshletVerticesGPU.Get(), &srvDesc, hDescriptor);


    // 다음 칸으로 이동
    hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

    // Polygon Primitives SRV (ByteAddressBuffer / Raw Buffer)
    // uint8 데이터는 RAW 버퍼로 읽어야 함
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Raw Buffer 필수 포맷
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    srvDesc.Buffer.StructureByteStride = 0; // Raw Buffer는 스트라이드 0
    // NumElements = 총 바이트 수 / 4 (4바이트 단위로 접근하기 때문)
    srvDesc.Buffer.NumElements = (UINT)polygonTriangles.size() / 4;

    md3dDevice->CreateShaderResourceView(mMeshletPrimsGPU.Get(), &srvDesc, hDescriptor);

    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    FlushCommandQueue();

    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));
}

void BoxApp::BuildPSO()
{
#pragma region Legacy IA
    /*D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
    ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
    psoDesc.pRootSignature = mRootSignature.Get();
    psoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), 
		mvsByteCode->GetBufferSize() 
	};
    psoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), 
		mpsByteCode->GetBufferSize() 
	};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = mBackBufferFormat;
    psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    psoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));*/
#pragma endregion

#pragma region Meshlet
    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC meshletPso;
    ZeroMemory(&meshletPso, sizeof(D3DX12_MESH_SHADER_PIPELINE_STATE_DESC));
    meshletPso.pRootSignature = mRootSignature.Get();
    meshletPso.MS =
    {
        reinterpret_cast<BYTE*>(mmsByteCode->GetBufferPointer()),
        mmsByteCode->GetBufferSize()
    };
    meshletPso.PS =
    {
        reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
        mpsByteCode->GetBufferSize()
    };
    meshletPso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    meshletPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    meshletPso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    meshletPso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    meshletPso.SampleMask = UINT_MAX;
    meshletPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    meshletPso.NumRenderTargets = 1;
    meshletPso.RTVFormats[0] = mBackBufferFormat;
    meshletPso.SampleDesc.Count = m4xMsaaState ? 4 : 1;
    meshletPso.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
    meshletPso.DSVFormat = mDepthStencilFormat;

    CD3DX12_PIPELINE_STATE_STREAM2 subobjectStream(meshletPso);

    D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc;
    pipelineStateStreamDesc.SizeInBytes = sizeof(subobjectStream);
    pipelineStateStreamDesc.pPipelineStateSubobjectStream = &subobjectStream;

    HRESULT hr = md3dDevice->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(mPSO.GetAddressOf()));
#pragma endregion
}