<p align="center">
  <img src="https://lh3.googleusercontent.com/d/1BtsxVaj1uNFQ2rkheULfgKAqEXA5SPfW" alt="Simple Icons" width=200>
  <h3 align="center">D3D12 Meshlet Sample</h3>
</p>

<p align="center">
  <img alt="Static Badge" src="https://img.shields.io/badge/Lang-C%2B%2B%2FDirectX%2012-blue">
  <img alt="Static Badge" src="https://img.shields.io/badge/Target-Windows-green">
</p>

<br>

## Preview
<p align="center">
  <img src="https://lh3.googleusercontent.com/d/1BtsxVaj1uNFQ2rkheULfgKAqEXA5SPfW" alt="Simple Icons" width=400>
  <img src="https://lh3.googleusercontent.com/d/1EL1vqrbs96enisJCK_u-FXedV5mr6EcS" alt="Simple Icons" width=400>
  <img src="https://lh3.googleusercontent.com/d/1UmkRW-Tjp-_INXw98wh0MJeohi5Hx8G-" alt="Simple Icons" width=400>
  <img src="https://lh3.googleusercontent.com/d/1-dc0EtRiXtzuGyZVD4K66-EW2sryGuW4" alt="Simple Icons" width=400>
  <img src="https://lh3.googleusercontent.com/d/13M0hHlQTlZPaaxb5bUBRIyco3jlDcFQF" alt="Simple Icons" width=400>
  <img src="https://lh3.googleusercontent.com/d/1XiDrGArgOWNv-Ps4DyuQSnRzUwDZZKZU" alt="Simple Icons" width=400>
  <img src="https://lh3.googleusercontent.com/d/1FY29Gr2lzVO6BDjnO6RzI2lZWDtvcU4p" alt="Simple Icons" width=400>
</p>

## Abstract
본 Repo는 Luna, Frank D. 저의 [DirectX 12를 이용한 3D 게임 프로그래밍 입문]에 포함된 Box 예제를 기반으로 Stanford Bunny, Zenless Zone Zero의 Jane Doe의 캐릭터 및 무기를 Meshlet 방식으로 렌더링하도록 수정한 샘플을 담고 있습니다. 

기존의 고정된 Input Assembler(IA) 단계가 갖는 병목을 해소하고 지오메트리 처리의 유연성을 확보하기 위해, 언리얼 엔진 5의 Nanite와 유사한 Meshlet 구조를 도입한 차세대 렌더링 파이프라인(Mesh Shader Pipeline)을 직접 설계 및 구현했습니다.

## Tech Stack
- **Language:** C++
- **Graphics API:** DirectX 12 (Mesh Shader)
- **Libraries:** meshoptimizer, FBX SDK

## Role & Highlights

### 1. Input Assembler(IA) 의존성 탈피
- 기존의 Vertex Buffer 바인딩 방식 대신, 1개의 CBV(Camera Data)와 4개의 SRV(```Vertices```, ```Meshlet Info```, ```Indices```, ```Primitives```)를 직접 바인딩합니다.
- 셰이더에서 Raw Data에 직접 접근하는 구조를 구현하여 GPU 친화적인 렌더링 파이프라인을 구축했습니다.

### 2. Meshlet 데이터 가공 파이프라인
- **FBX SDK**를 활용하여 범용 ```*.fbx``` 파일을 동적으로 로드합니다.
- **meshoptimizer** 라이브러리를 연동해 Vertex Cache 효율을 고려한 Meshlet 단위 클러스터링(Clustering) 및 데이터 직렬화를 수행합니다.

### 3. 차세대 셰이더 스테이지 제어
- 전통적인 IA/Vertex 단계 대신 **Mesh Shader 단계**를 제어하여 하드웨어 구조에 최적화된 렌더링 로직을 구현했습니다.
- Meshlet 클러스터링 시각화(Debug View) 기능을 통해 지오메트리 분할 및 처리 로직을 시각적으로 검증할 수 있습니다.

## Key Outcomes
- DirectX 12 환경에서 Mesh Shader Pipeline 아키텍처 바닥부터 구현 (IA Pipeline 제거)
- FBX 모델 로더 및 meshoptimizer 기반의 자동화된 Meshlet 생성 및 변환 시스템 구축
- 전통적인 Vertex Pipeline 대비 지오메트리 처리 효율 및 메모리 접근 패턴 최적화 연구 기반 마련
