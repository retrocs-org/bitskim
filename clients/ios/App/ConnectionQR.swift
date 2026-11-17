import SwiftUI
import AVFoundation
import CoreImage.CIFilterBuiltins

struct MobileInvitation: View {
    @ObservedObject var client: IOSClient
    @State private var address=""
    @State private var image: UIImage?
    @State private var expires=Date.distantPast
    var body: some View {
        MobileList {
            Section("Receive on this device") {
                Picker("LAN address",selection:$address){ForEach(client.addresses,id:\.self){Text($0).tag($0)}}
                Button("Generate connection QR"){
                    guard let text=client.invitation(address:address) else{return}
                    let filter=CIFilter.qrCodeGenerator();filter.message=Data(text.utf8);filter.correctionLevel="M"
                    if let output=filter.outputImage,let cg=CIContext().createCGImage(output,from:output.extent){image=UIImage(cgImage:cg);expires=Date().addingTimeInterval(300)}
                }.disabled(address.isEmpty)
                TimelineView(.periodic(from:.now,by:1)){context in
                    if context.date<expires,let image {Image(uiImage:image).interpolation(.none).resizable().scaledToFit().padding(16).background(Color.white);Text("Expires in \(Int(expires.timeIntervalSince(context.date))) seconds")}
                    else if image != nil {Text("Invitation expired. Generate a new code.")}
                }
                Text("Scan in BitSkim on the sending phone. The invitation checks this device’s identity. Incoming files still require approval.").font(.footnote)
            }
        }.navigationTitle("Connection QR").onAppear{address=client.addresses.first ?? ""}
    }
}
struct MobileQRScanner: View {
    let selected:(String)->Void
    @Environment(\.dismiss) private var dismiss
    @State private var text=""
    @State private var cameraError=""
    var body: some View {
        NavigationStack {
            VStack(spacing:16){
                CameraQRView(selected:selected,failure:{cameraError=$0}).frame(maxHeight:400).clipShape(RoundedRectangle(cornerRadius:16))
                if !cameraError.isEmpty{Text(cameraError).font(.footnote)}
                Text("Scan the connection QR shown by the receiving client, or paste its invitation.").font(.footnote)
                TextField("bitskim://…",text:$text).textInputAutocapitalization(.never).autocorrectionDisabled().textFieldStyle(.roundedBorder)
                Button("Review connection"){selected(text.trimmingCharacters(in:.whitespacesAndNewlines))}.disabled(text.isEmpty)
                Spacer()
            }.padding().navigationTitle("Scan connection QR").toolbar{ToolbarItem(placement:.cancellationAction){Button("Cancel"){dismiss()}}}
        }
    }
}
private struct CameraQRView: UIViewControllerRepresentable {
    let selected:(String)->Void
    let failure:(String)->Void
    func makeUIViewController(context:Context)->QRReaderController{let controller=QRReaderController();controller.selected=selected;controller.failure=failure;return controller}
    func updateUIViewController(_ controller:QRReaderController,context:Context){}
    static func dismantleUIViewController(_ controller:QRReaderController,coordinator:()){controller.stop()}
}
private final class QRReaderController:UIViewController,AVCaptureMetadataOutputObjectsDelegate {
    var selected:((String)->Void)?
    var failure:((String)->Void)?
    private let session=AVCaptureSession()
    private let worker=DispatchQueue(label:"org.bitskim.qr-camera")
    private var preview:AVCaptureVideoPreviewLayer?
    private var finished=false
    override func viewDidLoad(){super.viewDidLoad();view.backgroundColor = .black
        AVCaptureDevice.requestAccess(for:.video){[weak self] allowed in DispatchQueue.main.async {guard let self,!self.finished else{return};if allowed{self.configure()}else{self.failure?("Camera access is unavailable. You can paste a connection invitation instead.")}}}
    }
    private func configure(){
        guard let device=AVCaptureDevice.default(for:.video),let input=try? AVCaptureDeviceInput(device:device),session.canAddInput(input) else{failure?("No camera is available. Paste a connection invitation instead.");return}
        session.addInput(input);let output=AVCaptureMetadataOutput();guard session.canAddOutput(output) else{return};session.addOutput(output);output.setMetadataObjectsDelegate(self,queue:.main);output.metadataObjectTypes=[.qr]
        let layer=AVCaptureVideoPreviewLayer(session:session);layer.videoGravity = .resizeAspectFill;view.layer.addSublayer(layer);preview=layer;layer.frame=view.bounds
        worker.async{[session] in session.startRunning()}
    }
    override func viewDidLayoutSubviews(){super.viewDidLayoutSubviews();preview?.frame=view.bounds}
    func stop(){finished=true;worker.async{[session] in if session.isRunning{session.stopRunning()}}}
    func metadataOutput(_ output:AVCaptureMetadataOutput,didOutput objects:[AVMetadataObject],from connection:AVCaptureConnection){guard !finished,let text=(objects.first as? AVMetadataMachineReadableCodeObject)?.stringValue else{return};stop();selected?(text)}
}
