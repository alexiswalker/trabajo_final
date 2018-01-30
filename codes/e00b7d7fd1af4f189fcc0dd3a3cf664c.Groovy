package zhou.gank.io.data

import android.support.annotation.NonNull
import android.support.annotation.Nullable
import groovy.transform.CompileStatic
import zhou.gank.io.App
import zhou.gank.io.R
import zhou.gank.io.model.Gank
import zhou.gank.io.model.Result
import zhou.gank.io.net.NetworkManager
import zhou.gank.io.util.*

@CompileStatic
class TypeProvider implements DataProvider<List<Gank>> {

    private List<Gank> ganks
    private File file
    private String type, key
    private Pageable pageable
    private boolean noticeable

    TypeProvider(String type, int size) {
        this.type = type;
        key = HashKit.md5("${type}${size}-cache")
        file = new File(App.cacheFile(), key)
        pageable = new Pageable(1, size)
    }

    @Override
    void persistence() {
        if (hasLoad()) {
            new Thread({
                try {
                    FileKit.writeObject(file, new TypeGankEntity(pageable, ganks))
                } catch (Exception e) {
                    LogKit.d("persistence", "type", e)
                }
            }).start()
        }
    }

    @Nullable
    @Override
    List<Gank> get() {
        return ganks
    }

    @Override
    void set(@Nullable List<Gank> ganHuos, boolean more) {
        if (more && ganHuos != null && hasLoad()) {
            this.ganks.addAll(ganHuos)
        } else {
            this.ganks = ganHuos
        }
    }

    @Override
    void loadByCache(Closure closure) {
        def gks = null
        if (file.exists()) {
            new Thread({
                try {
                    def tge = FileKit.readObject(file) as TypeGankEntity
                    gks = tge.g
                    pageable = tge.p
                } catch (Exception e) {
                    LogKit.d("loadByCache", "type", e)
                }
            }).start()
            App.getInstance().getMainHandler().post({
                closure?.call(gks)
            })
        }else {
            closure?.call(gks)
        }
    }

    @Override
    void load(Closure closure, boolean more) {
        if (NetworkManager.getInstance().isNetworkConnected()) {
            if (more) {
                pageable.next()
            }
            NetworkKit.type(type, pageable.pageSize, pageable.pageNo, { result ->
                def gks = null
                if (result instanceof Result) {
                    def r = result as Result
                    if (r?.isSuccess()) {
                        gks = r.results
                    } else {
                        if (more) {
                            pageable.prev()
                        }
                        if (noticeable)
                            App.toast(R.string.failure_get)
                    }
                } else {
                    if (noticeable)
                        App.toast(result as String)
                }
                closure?.call(gks)
            })
        } else {
            if (noticeable)
                App.toast(R.string.error_network)
            closure?.call()
        }
    }

    @Override
    boolean hasLoad() {
        return ganks != null
    }

    @Override
    boolean needCache() {
        return true
    }

    @Override
    boolean clearCache() {
        return file.exists() && file.delete()
    }

    @NonNull
    @Override
    String key() {
        return key
    }

    @Override
    void setNoticeable(boolean noticeable) {
        this.noticeable = noticeable
    }

    public static class TypeGankEntity implements Serializable {

        public Pageable p
        public List<Gank> g

        TypeGankEntity(Pageable p, List<Gank> g) {
            this.p = p
            this.g = g
        }

        TypeGankEntity() {
        }
    }
}