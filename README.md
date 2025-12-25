# NDN-NAC Kubernetes Simulator

Kubernetes上でNDN (Named Data Networking) のメッシュネットワークと、NAC (Name-Based Access Control) をシミュレートするための環境構築ツールです

PythonとJinja2テンプレートを使用してKubernetesマニフェストを動的に生成し、NLSRによるルーティングや、証明書チェーンを用いたセキュアな通信、およびNACによるコンテンツアクセスコントロールの実験を行うことができます

## 全体の流れ
1. topology.yaml から NDN メッシュネットワークを生成
2. Kubernetes 上に NDN ノードをデプロイ
3. NFD / NLSR によりルーティングを確立
4. （Strict モード時）証明書チェーンによる検証を有効化
5. NAC を追加デプロイし、鍵配布とアクセス制御を実現

## 使用技術・用語
本シミュレータで利用されている主要な技術は以下の通りです
- **NDN(Named Data Networking)**
  - IPアドレスではなくコンテンツの名前を用いてパケットのやり取りを行う次世代ネットワークアーキテクチャの一つ
  - 本環境ではフォワーディングデーモンとして[NFD (NDN Forwarding Daemon)](https://github.com/named-data/NFD)を使用
- **NLSR(Named-data Link State Routing)**
  - NDNネットワーク内のルーティングプロトコル
  - 各ノードが自身の持つプレフィックスを広告し、経路計算を行います
  - [Github Page](https://github.com/named-data/NLSR)
- **NAC(Name-Based Access Control)**
  - コンテンツの名前空間を利用してアクセス制御を行う仕組み
  - Producer はコンテンツを共通鍵（CK）で暗号化して配信する
  - **AM(Access Manager)**がCKを暗号化する鍵(KEK)と復号鍵(KDK)を管理し、許可された Consumer のみがコンテンツを復号できるようにする
  - [Github Page](https://github.com/named-data/name-based-access-control)
  - [Specification](https://docs.named-data.net/NAC/latest/spec.html)

## 前提条件
- Docker
- Kubernetes Cluster
  - Minikube動作確認済み
- `kubectl` コマンドが使用可能であること

## ディレクトリ構成

```text
.
├── Dockerfile          # 実行環境用コンテナ定義
├── run.sh              # ツール実行用ラッパースクリプト
├── input/
│   ├── topology.yaml   # ノード間接続とセキュリティモードの定義
│   └── nac-policy.yaml # NACのアクセスポリシー定義
├── output/             # 生成されたマニフェストが出力される
├── templates/          # Jinja2テンプレート群 (NFD, NLSR, Deployment, Jobなど)
├── nac_src/            # NAC用アプリケーション (C++) ソースコード
├── main.py             # メッシュネットワーク構築用マニフェスト生成スクリプト
├── generate_nac.py     # NAC環境構築用マニフェスト生成スクリプト
└── test/               # テスト用スクリプト
```

## 使い方 (Usage)

### Phase1: メッシュネットワークの構築

NDNノードのデプロイとNLSRによるルーティング設定を行います

1. **トポロジーの設定**

   `input/topology.yaml` を編集し、ノード構成、隣接情報、セキュリティモード(`strict` or `lax`)を定義します

2. **マニフェストの生成**
   ```bash
   ./run.sh main.py
   ```
   `output/ndn-mesh.yaml`が生成されます

3. **デプロイ**
   ```bash
   kubectl apply -f output/ndn-mesh.yaml
   ```
   - **Setup Job**:証明書チェーンの作成とSecretへの登録を行います
   - **Nodes**:各NDNノードが起動し、NFD/NLSRが立ち上がります
4. **確認**
   ```bash
   kubectl get pods
   #ログの確認例
   kubectl logs job/ndn-setup-job
   kubectl logs <node-pod-name>
   ```

### Phase2: NAC(Named-based Access Control)の適用

Phase1で構築したネットワーク上にNACコンポーネントをデプロイします

1. **ポリシーの設定**

   `input/nac-policy.yaml`を編集し、Access Manager、配信するコンテンツ、許可するConsumerを定義します

2. **マニフェストの生成**
   ```bash
   ./run.sh generate_nac.py
   ```
   `output/ndn-nac.yaml`が生成されます
3. **デプロイ**
   ```bash
   kubectl apply -f output/ndn-nac.yaml
   ```
   - **Orchestrator Job**:各ノードへのソースコード配置、コンパイル、ポリシーに基づくKEK/KDKの生成と配置を自動で行います

## テスト・検証

`test/`ディレクトリ内のスクリプトを使用して動作確認を行います。`topology.yaml`で指定した**セキュリティモード**と、**NACを適用の有無**によって実行するテストスクリプトを選択してください

| 条件         | スクリプト              | 説明 |
|--------------|-------------------------|------|
| Laxモード    | `./test/test1_lax.sh`   | 証明書検証なし。正規でない鍵でもAdvertiseが可能であることを確認します。 |
| Strictモード | `./test/test2_strict.sh`| 証明書検証あり。正規証明書で通信成功し、不正証明書では拒否されることを確認します。 |
| NAC適用時    | `./test/test3_nac.sh`   | NACポリシー検証。許可されたConsumerのみがコンテンツを復号できることを確認します。 |

## Undeploy

環境を削除する場合は、生成したマニフェストを使用して削除してください
```bash
kubectl delete -f output/ndn-nac.yaml
kubectl delete -f output/ndn-mesh.yaml
```


