STM32 TinyML Magic Wand: LIS3DSH tabanlı Hareket Tanıma Sistemi

Bu çalışma, STM32F407G-DISC1 geliştirme kartı üzerinde bulunan yerleşik LIS3DSH ivmeölçer sensörünü kullanarak el hareketlerini gerçek zamanlı olarak sınıflandıran bir TinyML uygulamasıdır. Projenin temel amacı, düşük güç tüketimli bir mikrodenetleyici üzerinde karmaşık zaman serisi verilerini (Time-Series Data) işleyerek yapay zeka çıkarımı (Inference) gerçekleştirmektir.
Geliştirme Süreci ve Teknik Detaylar

Projenin donanım katmanı, STM32CubeMX üzerinden yapılandırılan SPI haberleşme hattı ve sonuçların izlenmesi için kurulan USB CDC (Virtual COM Port) protokolüne dayanmaktadır. LIS3DSH sensörünün donanımsal özellikleri, ivme verilerinin yüksek kararlılıkla okunmasını sağlamak amacıyla optimize edilmiştir. Özellikle verilerin "yırtılmasını" veya anlık gürültü sıçramalarını engellemek için tüm eksenlerin (X, Y, Z) tek bir SPI paketinde okunduğu "Block Read" mekanizması devreye alınmıştır.

Yapay zeka modelinin oluşturulması aşamasında Edge Impulse platformundan yararlanılmıştır. Veri toplama sürecinde Sabit, Dairesel ve Çizgisel hareketler için yeterli miktarda ham veri örneklenerek bir veri seti oluşturulmuştur. Bu veriler, Spectral Analysis blokları kullanılarak frekans domainine taşınmış ve anlamlı öznitelikler çıkarılmıştır. Eğitilen TensorFlow Lite Micro modeli, C++ kitaplığı olarak dışa aktarılmış ve STM32 projesine entegre edilmiştir.
Uygulama Mimarisi ve Çalışma Prensibi

Yazılımın ana döngüsü, sensörden gelen verileri belirli bir zaman penceresine (window) doldurarak çalışır. Bu pencere dolduğunda, gömülü sistem üzerindeki çıkarım motoru çalıştırılarak son iki saniyelik hareketin karakteristiği analiz edilir. Modelin sunduğu olasılık değerleri içerisinden en yüksek skora sahip olan etiket, belirlenen eşik değerinin (Threshold) üzerindeyse kullanıcı terminaline raporlanır.

Bu mimari sayesinde sistem, cihaz masada dururken "Sabit" durumunu korumakta, kullanıcı değneği salladığında ise yapılan hareketin tipini milisaniyeler içerisinde doğru bir şekilde tahmin edebilmektedir. Proje, özellikle uç cihazlarda yapay zeka (Edge AI) uygulamaları için bir konsept kanıtı (Proof of Concept) niteliğindedir.
Dosya Yapısı ve Kullanım

Proje dizininde yer alan Core/ klasörü uygulamanın ana mantığını ve inference döngüsünü barındırırken, EdgeImpulse/ klasörü eğitilmiş modeli ve gerekli SDK dosyalarını içermektedir. TintML.ioc dosyası ise tüm donanım pinlerinin ve çevre birimlerinin konfigürasyonunu saklar. Uygulamayı çalıştırmak için kartı bilgisayara bağlayıp herhangi bir seri terminal üzerinden 115200 baud hızıyla sonuçları izlemek yeterlidir.

Model Performansı ve Doğrulama

Eğitim sürecinde toplanan veriler, modelin genelleme yeteneğini ölçmek amacıyla eğitim ve test setleri olarak ikiye ayrılmıştır. Yapılan optimizasyonlar sonucunda, modelin daha önce görmediği veriler üzerindeki başarı oranını temsil eden Test Doğruluğu (Test Accuracy) %78 olarak kaydedilmiştir. Bu oran, kısıtlı donanım kaynaklarına sahip bir mikrodenetleyici üzerinde çalışan bir sınıflandırma modeli için oldukça tatmin edici bir seviyededir. Eğitim setinde elde edilen yüksek doğruluk oranları ile test sonuçları arasındaki bu denge, modelin "overfitting" (aşırı öğrenme) riskinden uzak, kararlı bir yapıda olduğunu ve gerçek dünya senaryolarında farklı açılardan gelen hareketleri başarıyla ayırt edebildiğini kanıtlamaktadır.
